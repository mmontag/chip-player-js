
struct sound_driver {
	char *id;
	char *description;
	char **help;
	int (*init) ();
        void (*deinit) ();
        void (*pause) ();
        void (*resume) ();
        struct list_head *next;
};
