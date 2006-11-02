
extern	int		numthreads;

void ThreadSetDefault (void);
int	GetThreadWork (void);
void RunThreadsOnIndividual (int workcnt, qboolean showpacifier, void(*func)(unsigned int));
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(unsigned int));
void ThreadLock (void);
void ThreadUnlock (void);

