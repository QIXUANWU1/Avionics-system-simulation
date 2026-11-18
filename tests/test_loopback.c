#define _POSIX_C_SOURCE 200809L
#include "linux_lab.h"
#include <signal.h>
#include <inttypes.h>
#include <sys/prctl.h>
#include <sys/wait.h>

/* Starts only short-lived local test children; the only sockets are loopback. */
enum { MAX_WORKERS=32, PROFILE=1, DURATION_MS=8200 };
static pid_t children[MAX_WORKERS];
static pid_t recorder=-1;

static int wait_child(pid_t *pid,uint64_t deadline)
{
    int status=0;
    const struct timespec gap={0,10000000};
    while (lab_now()<deadline) {
        const pid_t result=waitpid(*pid,&status,WNOHANG);
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
                          const char *duration)
{
    const pid_t parent=getpid();
    const pid_t pid=fork();
    if (!pid) {
        if (prctl(PR_SET_PDEATHSIG,SIGTERM,0UL,0UL,0UL)<0 || getppid()!=parent) _exit(124);
        execl("build/ima-worker","ima-worker","1",domain,base,session,start,duration,"1",(char *)NULL);
        _exit(126);
    }
    return pid;
}

static int check_trace(FILE *trace)
{
    unsigned seen[SIM_SERVICES][SIM_SERVICES]={0};
    unsigned pfd_reacted=0,pfd_centered=0,positive_model=0,engine=0,weather=0,count=0;
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
        if (m.kind==KIND_SAMPLE && m.flags==1) {
            if (m.sid==SID_primary_display && m.model_ms>=2500 && m.model_ms<4000 && m.v[0]>7.0) pfd_reacted++;
            if (m.sid==SID_primary_display && m.model_ms>=6200 && m.model_ms<7500 && fabs(m.v[0])<1.75) pfd_centered++;
            if (m.sid==SID_engine_display) engine++;
            if (m.sid==SID_weather_display) weather++;
        }
        if (m.kind==KIND_ADVISORY && (m.flags&1u)) positive_model++;
    }
    static const unsigned required[][2]={
        {SID_pilot_axes,SID_attitude_reference},{SID_attitude_reference,SID_control_effort},
        {SID_control_effort,SID_actuator_response},{SID_actuator_response,SID_plant_attitude},
        {SID_plant_attitude,SID_imu_sample},{SID_imu_sample,SID_attitude_estimate},
        {SID_attitude_estimate,SID_control_effort},{SID_attitude_estimate,SID_primary_display},
        {SID_plant_motion,SID_baro_fixture},{SID_fuel_quantity,SID_engine_display},
        {SID_airdata_words,SID_airdata_decoded},{SID_airdata_decoded,SID_primary_display},
        {SID_bus_status,SID_flight_mode}
    };
    for (size_t i=0;i<sizeof required/sizeof *required;i++) {
        if (!seen[required[i][0]][required[i][1]]) {
            fprintf(stderr,"missing read: %u -> %u\n",required[i][0],required[i][1]); return 0;
        }
    }
    printf("Loopback trace: %u records, %u response / %u centered PFD samples, %u engine, %u weather and %u advisory samples.\n",
           count,pfd_reacted,pfd_centered,engine,weather,positive_model);
    return pfd_reacted && pfd_centered && positive_model && engine && weather;
}

int main(void)
{
    const unsigned n=ima_domain_count[PROFILE];
    if (!n || n>MAX_WORKERS) return 1;
    const unsigned port=select_base(n);
    FILE *const trace=port?tmpfile():NULL;
    if (!trace) { fputs("No free test ports or temporary trace file.\n",stderr); return 1; }
    char base[16],start[32],duration[16];
    const char *session="1643317310";
    const uint64_t start_ns=lab_now()+UINT64_C(800000000);
    (void)snprintf(base,sizeof base,"%u",port);
    (void)snprintf(start,sizeof start,"%" PRIu64,start_ns);
    (void)snprintf(duration,sizeof duration,"%d",DURATION_MS);
    int ok=0;
    recorder=start_recorder(trace,base,session);
    if (recorder<=0) goto done;
    for (unsigned d=0;d<n;d++) {
        char domain[16];
        (void)snprintf(domain,sizeof domain,"%u",d);
        children[d]=start_worker(domain,base,session,start,duration);
        if (children[d]<=0) goto done;
    }
    const uint64_t deadline=start_ns+((uint64_t)DURATION_MS+2500u)*UINT64_C(1000000);
    for (unsigned d=0;d<n;d++) if (!wait_child(&children[d],deadline)) goto done;
    /* Allow the reader to receive the last already-sent datagrams. */
    const struct timespec drain={0,300000000};
    nanosleep(&drain,NULL);
    if (kill(recorder,SIGTERM)<0 || !wait_child(&recorder,lab_now()+UINT64_C(2000000000))) goto done;
    ok=check_trace(trace);
done:
    stop_child(&recorder);
    for (unsigned d=0;d<n;d++) stop_child(&children[d]);
    fclose(trace);
    return !ok;
}
