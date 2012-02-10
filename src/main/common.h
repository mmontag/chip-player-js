
struct options {
	int start;		/* start order */
	int amplify;		/* amplification factor */
	int freq;		/* sampling rate */
	int format;		/* sample format */
	int time;		/* max. replay time */
	int mix;		/* channel separation */
	int loop;		/* loop module */
	int random;		/* play in random order */
	int load_only;		/* load module and exit */
	char *out_file;		/* output file name */
	char *ins_path;		/* instrument path */
};

int set_tty(void);
int reset_tty(void);

