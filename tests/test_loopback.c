#define _GNU_SOURCE
#include "linux_lab.h"
#include <signal.h>
#include <inttypes.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/wait.h>

/* Starts only short-lived local test children; the only sockets are loopback. */
enum { MAX_WORKERS=32, PROFILE=1, DURATION_MS=8200 };
static pid_t children[MAX_WORKERS];
static pid_t recorder=-1,panel=-1;

static int wait_child(pid_t *pid,uint64_t deadline,struct rusage *usage)
{
    int status=0;
    const struct timespec gap={0,10000000};
    while (lab_now()<deadline) {
        const pid_t result=wait4(*pid,&status,WNOHANG,usage);
        if (result==*pid) { *pid=-1; return WIFEXITED(status)&&WEXITSTATUS(status)==0; }
        if (result<0 && errno!=EINTR) return 0;
        nanosleep(&gap,NULL);
    }
    return 0;
}

static void stop_child(pid_t *pid)
{
    if (*pid<=0) return;
    (void)kill(*pid,SIGKILL);
    while (waitpid(*pid,NULL,0)<0 && errno==EINTR) {}
    *pid=-1;
}

static unsigned select_base(unsigned count)
{
    unsigned base=33000u+(unsigned)getpid()%23000u;
    for (unsigned attempt=0;attempt<20;attempt++,base+=33) {
        int fds[MAX_WORKERS+1];
        unsigned n=0;
        for (;n<=count;n++) {
            const unsigned port=n<count?IMA_DOMAIN_PORT(base,n):IMA_OBSERVE_PORT(base);
            fds[n]=lab_bind(port,SOCK_NONBLOCK);
            if (fds[n]<0) break;
        }
        for (unsigned i=0;i<n;i++) close(fds[i]);
        if (n>count) return base;
    }
    return 0;
}

static pid_t start_recorder(FILE *trace,const char *base,const char *session)
{
    const pid_t parent=getpid();
    const pid_t pid=fork();
    if (!pid) {
        if (prctl(PR_SET_PDEATHSIG,SIGTERM,0UL,0UL,0UL)<0 || getppid()!=parent) _exit(124);
        if (dup2(fileno(trace),STDOUT_FILENO)<0) _exit(125);
        if (fileno(trace)!=STDOUT_FILENO) (void)close(fileno(trace));
        execl("build/ima-record-mirror","ima-record-mirror","1",base,session,(char *)NULL);
        _exit(126);
    }
    return pid;
}

static pid_t start_worker(const char *domain,const char *base,const char *session,const char *start,
                          const char *duration,const char *transport)
{
    const pid_t parent=getpid();
    const pid_t pid=fork();
    if (!pid) {
        if (prctl(PR_SET_PDEATHSIG,SIGTERM,0UL,0UL,0UL)<0 || getppid()!=parent) _exit(124);
        execl("build/ima-worker","ima-worker","1",domain,base,session,start,duration,"1",transport,(char *)NULL);
        _exit(126);
    }
    return pid;
}

static pid_t start_panel(const char *base,const char *session,const char *start,const char *duration)
{
    const pid_t parent=getpid();
    const pid_t pid=fork();
    if (!pid) {
        if (prctl(PR_SET_PDEATHSIG,SIGTERM,0UL,0UL,0UL)<0 || getppid()!=parent) _exit(124);
        execl("build/ima-panel","ima-panel","--live","1",base,session,start,duration,"build/panel-live.svg",(char *)NULL);
        _exit(126);
    }
    return pid;
}

static int read_panel(const char *file,int live)
{
    FILE *f=fopen("build/panel-live.svg","rb");
    if (!f) return 0;
    char b[128000];
    const size_t n=fread(b,1,sizeof b-1,f);
    const int good=n&&feof(f)&&!ferror(f);
    fclose(f);
    b[n]='\0';
    if (!good||!strstr(b,"DDS 在线")||!strstr(b,"</svg>") || !strstr(b,"id='navigation'") || !strstr(b,"id='systems'")) return 0;
    if (!live) return strstr(b,"姿态数据无效")&&strstr(b,"画面有效")==NULL;
    const char speed_tag[]="<text x='112' y='390' class='tape middle'>";
    const char *speed=strstr(b,speed_tag);
    if (!speed) return 0;
    char *end;
    const long knots=strtol(speed+sizeof speed_tag-1,&end,10);
    if (knots<90||knots>200||strncmp(end,"</text>",7)) return 0;
    if (strstr(b,"姿态数据无效")||!strstr(b,"画面有效")||!strstr(b,"候选回波")||
        !strstr(b,"惯性参考  双路有效")||!strstr(b,"影子计算  一致")||!strstr(b,"ft/min")) return 0;
    f=fopen(file,"wb");
    if (!f) return 0;
    const int written=fwrite(b,1,n,f)==n;
    return fclose(f)==0&&written;
}

static double cpu_ms(const struct rusage *usage)
{
    return 1000.0*(usage->ru_utime.tv_sec+usage->ru_stime.tv_sec)
           +.001*(usage->ru_utime.tv_usec+usage->ru_stime.tv_usec);
}

static int check_trace(FILE *trace)
{
    unsigned seen[SIM_SERVICES][SIM_SERVICES]={0};
    unsigned received[SIM_SERVICES][MAX_WORKERS]={0};
    uint64_t valid=0;
    unsigned pfd_reacted=0,pfd_centered=0,positive_model=0,engine=0,weather=0,count=0,gear_transition=0,gear_retracted=0;
    unsigned char prefix[8],record[9+WIRE_SIZE];
    rewind(trace);
    if (fread(prefix,1,sizeof prefix,trace)!=sizeof prefix || memcmp(prefix,"IMATRCE1",8)) return 0;
    for (;;) {
        const size_t n=fread(record,1,sizeof record,trace);
        if (!n) { if (ferror(trace)) return 0; break; }
        Message m;
        if (n!=sizeof record || wire_decode(record+9,WIRE_SIZE,&m) || record[0]!=m.kind) return 0;
        count++;
        if (m.kind==KIND_CONSUME && (m.flags&1u) && m.sid<SIM_SERVICES && m.flags>>8<SIM_SERVICES)
            seen[m.sid][m.flags>>8]=1;
        if (m.kind==KIND_RECEIVE && m.sid<SIM_SERVICES && m.flags<MAX_WORKERS)
            received[m.sid][m.flags]=1;
        if (m.kind==KIND_SAMPLE && m.sid<SIM_SERVICES && m.flags==1) {
            valid|=UINT64_C(1)<<m.sid;
            if (m.sid==SID_primary_display && m.model_ms>=2500 && m.model_ms<4000 && m.v[0]>7.0) pfd_reacted++;
            if (m.sid==SID_primary_display && m.model_ms>=6200 && m.model_ms<7500 && fabs(m.v[0])<1.75) pfd_centered++;
            if (m.sid==SID_engine_display) engine++;
            if (m.sid==SID_weather_display) weather++;
            if (m.sid==SID_gear_indication && m.model_ms>1000 && m.model_ms<2100 && m.v[0]==2) gear_transition++;
            if (m.sid==SID_gear_indication && m.model_ms>=2300 && m.model_ms<4000 && m.v[0]==0) gear_retracted++;
        }
        if (m.kind==KIND_ADVISORY && (m.flags&1u)) positive_model++;
    }
    static const unsigned required[][2]={
        {SID_pilot_axes,SID_attitude_reference},{SID_attitude_reference,SID_control_effort},
        {SID_control_effort,SID_actuator_response},{SID_actuator_response,SID_plant_attitude},
        {SID_plant_attitude,SID_imu_sample},{SID_imu_sample,SID_attitude_select},{SID_attitude_select,SID_attitude_estimate},
        {SID_attitude_estimate,SID_control_effort},{SID_attitude_estimate,SID_primary_display},
        {SID_plant_motion,SID_baro_fixture},{SID_fuel_quantity,SID_engine_display},
        {SID_airdata_words,SID_airdata_decoded},{SID_airdata_decoded,SID_primary_display},
        {SID_bus_status,SID_flight_mode},{SID_imu_backup,SID_attitude_select},
        {SID_imu_backup,SID_shadow_effort},{SID_shadow_effort,SID_control_agreement},
        {SID_cabin_lighting,SID_camera_frame_source},{SID_bus_load,SID_electrical_display},
        {SID_plant_motion,SID_gnss_fixture},{SID_gnss_fixture,SID_navigation_display},
        {SID_gear_position_fixture,SID_gear_indication},
        {SID_plant_motion,SID_position_fixture},{SID_position_fixture,SID_gnss_position},
        {SID_gnss_position,SID_route_indication}
    };
    for (size_t i=0;i<sizeof required/sizeof *required;i++) {
        const unsigned src=required[i][0],dst=required[i][1],d=ima_owner[PROFILE][dst];
        if (!seen[src][dst]||(ima_owner[PROFILE][src]!=d&&!received[src][d])) {
            fprintf(stderr,"missing read: %u -> %u\n",required[i][0],required[i][1]); return 0;
        }
    }
    printf("Loopback trace: %u records, %u response / %u centered PFD samples, %u engine, %u weather and %u advisory samples.\n",
           count,pfd_reacted,pfd_centered,engine,weather,positive_model);
    for (unsigned i=0;i<SIM_SERVICES;i++)
        if (!(valid&(UINT64_C(1)<<i))) {fprintf(stderr,"no valid publication for service %u\n",i);return 0;}
    printf("All %u basic services produced a valid sample; required cross-domain receives and reads verified.\n",(unsigned)SIM_SERVICES);
    printf("Landing gear sensor path: %u in transit, %u retracted confirmations.\n",
           gear_transition,gear_retracted);
    return pfd_reacted && pfd_centered && positive_model && engine && weather && gear_transition && gear_retracted;
}

int main(int argc,char **argv)
{
    const char *transport=argc==2?argv[1]:"dds";
    if (argc>2 || (strcmp(transport,"dds") && strcmp(transport,"udp"))) return 2;
    const unsigned n=ima_domain_count[PROFILE];
    if (!n || n>MAX_WORKERS) return 1;
    const unsigned port=select_base(n);
    FILE *const trace=port?fopen(!strcmp(transport,"dds")?"build/loopback-dds.bin":"build/loopback-udp.bin","w+b"):NULL;
    if (!trace) { fputs("No free test ports or temporary trace file.\n",stderr); return 1; }
    char base[16],start[32],duration[16];
    const char *session="1643317310";
    const uint64_t start_ns=lab_now()+UINT64_C(1800000000);
    (void)snprintf(base,sizeof base,"%u",port);
    (void)snprintf(start,sizeof start,"%" PRIu64,start_ns);
    (void)snprintf(duration,sizeof duration,"%d",DURATION_MS);
    int ok=0;
    const int check_live=!strcmp(transport,"dds");
    struct rusage usage;
    double worker_cpu=0;
    long worker_rss=0;
    recorder=start_recorder(trace,base,session);
    if (recorder<=0) goto done;
    for (unsigned d=0;d<n;d++) {
        char domain[16];
        (void)snprintf(domain,sizeof domain,"%u",d);
        children[d]=start_worker(domain,base,session,start,duration,transport);
        if (children[d]<=0) goto done;
    }
    if (check_live) {
        (void)unlink("build/panel-live.svg");
        panel=start_panel(base,session,start,duration);
        if (panel<=0) goto done;
        const uint64_t sample_time=start_ns+UINT64_C(2250000000);
        while (lab_now()<sample_time) {
            const struct timespec gap={.tv_nsec=20000000};
            nanosleep(&gap,NULL);
        }
        if (!read_panel("build/panel-dds-capture.svg",1)) {
            fputs("Live panel did not show the DDS service values.\n",stderr);goto done;
        }
    }
    const uint64_t deadline=start_ns+((uint64_t)DURATION_MS+2500u)*UINT64_C(1000000);
    for (unsigned d=0;d<n;d++) {
        if (!wait_child(&children[d],deadline,&usage)) goto done;
        worker_cpu+=cpu_ms(&usage);
        if (usage.ru_maxrss>worker_rss) worker_rss=usage.ru_maxrss;
    }
    printf("Workers (%u, %.1f s model): %.2f ms total user+system CPU, %ld KiB maximum individual peak RSS.\n",
           n,DURATION_MS/1000.0,worker_cpu,worker_rss);
    if (check_live) {
        if (!wait_child(&panel,deadline,&usage)||!read_panel(NULL,0)) {
            fputs("Live panel did not blank stale values after workers stopped.\n",stderr);goto done;
        }
        printf("Live panel: fresh DDS values and stale blanking passed; %.2f ms CPU, %ld KiB peak RSS.\n",
               cpu_ms(&usage),usage.ru_maxrss);
    }
    /* Allow the reader to receive the last already-sent datagrams. */
    const struct timespec drain={0,300000000};
    nanosleep(&drain,NULL);
    if (kill(recorder,SIGTERM)<0 || !wait_child(&recorder,lab_now()+UINT64_C(2000000000),&usage)) goto done;
    printf("Mirror: %.2f ms CPU, %ld KiB peak RSS.\n",cpu_ms(&usage),usage.ru_maxrss);
    ok=check_trace(trace);
done:
    stop_child(&panel);
    stop_child(&recorder);
    for (unsigned d=0;d<n;d++) stop_child(&children[d]);
    fclose(trace);
    return !ok;
}
