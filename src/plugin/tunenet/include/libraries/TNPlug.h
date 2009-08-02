#ifndef lib_TNPlugCommon
#define lib_TNPlugCommon

/* TuneNet API Version 1.06 */
#define	PLUGIN_API_VERSION	10600						// Version 1.04

/* Defines for PlayModes */
#define	PLM_File				1								// ** Standard File **
#define	PLM_FileLoad		2								// ** Load in all at once **
#define	PLM_Stream			4								// ** Char buffer to a http stream **
#define	PLM_ContainerFile	8								// ** Information stored in a container file ** (PLS)
#define	PLM_Container		16								// ** Information stored in a buffer **

/* Player bit flags */
#define aPlayer_SAVE_stream	1							// ** Basic Save (MP3 Only) **
#define aPlayer_SAVE_convert	2							
#define aPlayer_SEEK				4							// ** Can Seek **

#define aPlayer_IsDECODE		64							// ** Will be set by default if nothing else is **
#define aPlayer_IsENCODE		128						// ** **
#define aPlayer_IsBROADCAST	256						// ** **

/* Player Priority flags (API - 1.06) */
#define TN_PRI_Passive		-127
#define TN_PRI_Normal			0

/* Render Modes */
#define	ARender_Direct				1						// ** DO NOT USE! **
#define	ARender_InternalBuffer	2						// ** PCM allocated by the player **

/* Notify Events */
#define TNDN_SubSong				500						// ** Sets a new sub song to play **
#define TNDN_PlaySpeed			501						

/* Left / Right Mix (Please stick to just these values, future compatibility cannot be gaurenteed otherwise) */
#define	TN_STEREO						0
#define	TN_STEREO_25PER				12
#define	TN_STEREO_50PER				25
#define	TN_STEREO_75PER				38
#define	TN_MONO							50
#define	TN_REVERSE_STEREO_25PER		62
#define	TN_REVERSE_STEREO_50PER		75
#define	TN_REVERSE_STEREO_75PER		88
#define	TN_REVERSE_STEREO				100

#define TN_TUNE_END				-1
#define TN_TUNE_BAD				-2
#define TN_TUNE_MEM				-3
#define TN_TUNE_NOSYNC			-4
#define TN_TUNE_BADVAL			-5
#define TN_TUNE_END_NEXT_SUB	-6

/* The TuneNet structure is allocated by TuneNet and passed to the Player.

	Please take notes of the values you are allowed to change.
*/

struct TuneNet  	//MP3StreamData
{
	/* Error messages reported from Player in Real Time: *WRITE* by Plugin Player Only (TuneNet will Read) */
	volatile	int	lastmessage;					// ** Last Error reported by Stream Read **
	volatile	int	exterror;						// ** Error II (eg: ICY errors) **

	/* Preload Space (READ ONLY FROM PLUGIN PLAYERS) */
	BYTE		* in_file;								// ** File In (If preload) **
	ULONG		in_file_size;							// ** Size of File **
	ULONG		in_file_offset;						// ** Offset **
	
	int		playmode;								// ** READ ONLY - Contains Current Playmode ** 
	
	/* Tune Strings (WRITE ONLY FROM PLUGIN PLAYERS) */	
	char		ST_Name[255];							// Station Name - Copy Null termianted 255 byte string here 
	char		ST_Url[255];							// URL - Copy Null terminated 255 byte string here
	char		ST_Genre[255];							// GENRE - Copy Null terminated 255 byte string here
	char		Tune[255];								// Tune Name - Copy Null terminated 255 byte string here
	char		ST_Note[255];							// Notes, also contains any customer error messages **

	/* Audio Details - Set by Players only */
	APTR		handle;									// ** Internal player handle */
	ULONG		DirectRender;							// ** Must be set to Direct Render 2 mode at the moment **
	ULONG		dec_frequency;							// eg: 44100
	ULONG		dec_channels;							// eg: 2
	ULONG		dec_quality;							// ** Depreciated **
	ULONG		ms_duration;							// 0 = no duration, otherwise time given in milliseconds
	ULONG		bitrate;									// kbps rate

	ULONG		versionmajor;							// eg: MP3
	ULONG		versionminor;							// eg: 2 (mp3.2)

	WORD	*	pcm[2];									// ** 16 Bit Audio (2 channels) - MUST BE ALLOCATED BY THE PLUGIN PLAYER **
	
	ULONG		max_subsongs;							// ** Max Songs 0 or 1 = single file, 2+ = multiple sub songs (default = 0)
	ULONG		current_subsong;						// ** Current song starting from 0 (default = 0)
	int		mix_lr;									// ** Suggested Initial Mix LR (can be overridden by user) (See Left and Right Mix defines above) **
	BOOL		songEOF;									// ** Set to TRUE if priority given to EOF over Timer (1) - doesn't work with X-FADE **	
};

#define	ad_record		1
#define	ad_broadcast	2

struct audio_decode_mode
{
	char		modename[32];									// ** Must be unique within decoder **
	uint32	flags;											// ** individual modes ad_broadcast, ad_record, ad_quality **
	uint32	frequency;
	uint16	kbps;
	uint8		channels;
	uint8		qualityRecord;
	uint8		qualityBroadcast;
	char		* fileext;										// ** Audio extension to file (pointers, as usually the same)
	char		* castext;										// ** Broadcast extention 
};

struct audio_decode
{
	uint32	flags;
	uint32	defaultMode;
	uint32	res2;
	uint32	modeVersion;
	struct	audio_decode_mode * modes;
};

/* Audio Players */
struct audio_player
{
	char	name[127];											// ** Name of player **
	char	author[127];
	char	description[127];
	char	ext[16];												// ** Extentions (.mp3) comma seperated (mp3,mp2) **
	char	*	patternmatch;									// ** Pointer to pattern match string **
	struct Library * 	ap_library;							// ** This Library Pointer   - Leave as NULL
	struct Interface * ap_interface;						// ** This Interface Pointer - Leave as NULL
	BOOL	extANDpmatch;										// ** If not set then one or the other **
	uint8	playmodes;											// ** Modes of play (Or'ed together) **
	uint8 playerflags;										// ** Abilities - SAVE etc..... **
	LONG  plugin_api_version;								// ** V01.02 = 010200
	LONG	plugin_version;									// ** Your Revision in MMmmss (Major, Minor and Sub Format) **
	struct audio_decode * decode_list;					// ** Pointer to list of encode modes 
	int8	priority;											// ** (From V1.06) Priority of plugin (See above for valid values) **
	uint8	additionalflags;
};

struct TuneNet_Enc_Info
{
	ULONG	frequency;
	UWORD	kbps;
	UBYTE	channels;
	UBYTE	quality;
};

struct TuneNet_Encode_Prefs
{
	struct	TuneNet_Enc_Info Input;
	struct	TuneNet_Enc_Info Output;
};


struct TuneNet_Encode
{
	APTR		handle;											// ** Handle **
	APTR		encode_buffer;									// ** Buffer **
	ULONG		encode_buffer_size;							// ** Max Size **
	char		save_ext[8];									// ** . save extension **
};

struct TuneNet_Encode_File_Block
{
	int64		offset;
	uint16	length;
	uint16	spare;
	uint8		block[2];										// ** blocks are varying size **
};

#endif
