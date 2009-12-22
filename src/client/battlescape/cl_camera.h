
extern cvar_t *cl_centerview;
extern cvar_t *cl_camzoommax;
extern cvar_t *cl_camzoommin;
extern cvar_t *cl_camzoomquant;

void CL_CameraInit(void);
void CL_CameraMove(void);
void CL_CameraRoute(const pos3_t from, const pos3_t target);
