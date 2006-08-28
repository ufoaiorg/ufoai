/*============================================================================ */

typedef struct sizebuf_s {
	qboolean allowoverflow;		/* if false, do a Com_Error */
	qboolean overflowed;		/* set to true if the buffer size failed */
	byte *data;
	int maxsize;
	int cursize;
	int readcount;
} sizebuf_t;

void SZ_Init(sizebuf_t * buf, byte * data, int length);
void SZ_Clear(sizebuf_t * buf);
void *SZ_GetSpace(sizebuf_t * buf, int length);
void SZ_Write(sizebuf_t * buf, void *data, int length);
void SZ_Print(sizebuf_t * buf, char *data);	/* strcats onto the sizebuf */

/*============================================================================ */

struct usercmd_s;
struct entity_state_s;

void MSG_WriteChar(sizebuf_t * sb, int c);

#ifdef DEBUG
#define MSG_WriteByte(buffer, char) MSG_WriteByteDebug( buffer, char, __FILE__, __LINE__ )
#define MSG_WriteShort(buffer, char) MSG_WriteShortDebug( buffer, char, __FILE__, __LINE__ )
void MSG_WriteByteDebug(sizebuf_t * sb, int c, char *file, int line);
void MSG_WriteShortDebug(sizebuf_t * sb, int c, char* file, int line);
#else
void MSG_WriteByte(sizebuf_t * sb, int c);
void MSG_WriteShort(sizebuf_t * sb, int c);
#endif
void MSG_WriteLong(sizebuf_t * sb, int c);
void MSG_WriteFloat(sizebuf_t * sb, float f);
void MSG_WriteString(sizebuf_t * sb, char *s);
void MSG_WriteCoord(sizebuf_t * sb, float f);
void MSG_WritePos(sizebuf_t * sb, vec3_t pos);
void MSG_WriteGPos(sizebuf_t * sb, pos3_t pos);
void MSG_WriteAngle(sizebuf_t * sb, float f);
void MSG_WriteAngle16(sizebuf_t * sb, float f);
void MSG_WriteDir(sizebuf_t * sb, vec3_t vector);
void MSG_V_WriteFormat(sizebuf_t * sb, char *format, va_list ap);
void MSG_WriteFormat(sizebuf_t * sb, char *format, ...);

void MSG_BeginReading(sizebuf_t * sb);

int MSG_ReadChar(sizebuf_t * sb);
int MSG_ReadByte(sizebuf_t * sb);
int MSG_ReadShort(sizebuf_t * sb);
int MSG_ReadLong(sizebuf_t * sb);
float MSG_ReadFloat(sizebuf_t * sb);
char *MSG_ReadString(sizebuf_t * sb);
char *MSG_ReadStringLine(sizebuf_t * sb);

float MSG_ReadCoord(sizebuf_t * sb);
void MSG_ReadPos(sizebuf_t * sb, vec3_t pos);
void MSG_ReadGPos(sizebuf_t * sb, pos3_t pos);
float MSG_ReadAngle(sizebuf_t * sb);
float MSG_ReadAngle16(sizebuf_t * sb);
void MSG_ReadDir(sizebuf_t * sb, vec3_t vector);

void MSG_ReadData(sizebuf_t * sb, void *buffer, int size);
void MSG_V_ReadFormat(sizebuf_t * sb, char *format, va_list ap);
void MSG_ReadFormat(sizebuf_t * sb, char *format, ...);

int MSG_LengthFormat(sizebuf_t * sb, char *format);

/*============================================================================ */

extern qboolean bigendien;

extern short BigShort(short l);
extern short LittleShort(short l);
extern int BigLong(int l);
extern int LittleLong(int l);
extern float BigFloat(float l);
extern float LittleFloat(float l);

/*============================================================================ */


int COM_Argc(void);
char *COM_Argv(int arg);		/* range and null checked */
void COM_ClearArgv(int arg);
int COM_CheckParm(char *parm);
void COM_AddParm(char *parm);

void COM_Init(void);
void COM_InitArgv(int argc, char **argv);

char *CopyString(char *in);

/*============================================================================ */


