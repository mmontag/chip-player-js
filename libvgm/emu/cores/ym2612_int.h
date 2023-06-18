#ifndef __YM2612_INT_H__
#define __YM2612_INT_H__

// --- Gens YM2612 - header for internal stuff---
#include "ym2612.h"

// Change it if you need to do long update
#define	MAX_UPDATE_LENGTH   0x200

// Gens always uses 16 bits sound (in 32 bits buffer) and do the convertion later if needed.
#define OUTPUT_BITS         15
// OUTPUT_BITS 15 is MAME's volume level
#define DAC_SHIFT           (OUTPUT_BITS - 9)
// DAC_SHIFT makes sure that FM and DAC volume has the same volume

typedef struct slot__ {
	int *DT;	// paramètre detune
	int MUL;	// paramètre "multiple de fréquence"
	int TL;		// Total Level = volume lorsque l'enveloppe est au plus haut
	int TLL;	// Total Level ajusted
	int SLL;	// Sustin Level (ajusted) = volume où l'enveloppe termine sa première phase de régression
	int KSR_S;	// Key Scale Rate Shift = facteur de prise en compte du KSL dans la variations de l'enveloppe
	int KSR;	// Key Scale Rate = cette valeur est calculée par rapport à la fréquence actuelle, elle va influer
				// sur les différents paramètres de l'enveloppe comme l'attaque, le decay ...  comme dans la réalité !
	int SEG;	// Type enveloppe SSG
	int *AR;	// Attack Rate (table pointeur) = Taux d'attaque (AR[KSR])
	int *DR;	// Decay Rate (table pointeur) = Taux pour la régression (DR[KSR])
	int *SR;	// Sustin Rate (table pointeur) = Taux pour le maintien (SR[KSR])
	int *RR;	// Release Rate (table pointeur) = Taux pour le relâchement (RR[KSR])
	int Fcnt;	// Frequency Count = compteur-fréquence pour déterminer l'amplitude actuelle (SIN[Finc >> 16])
	int Finc;	// frequency step = pas d'incrémentation du compteur-fréquence
				// plus le pas est grand, plus la fréquence est aïgu (ou haute)
	int Ecurp;	// Envelope current phase = cette variable permet de savoir dans quelle phase
				// de l'enveloppe on se trouve, par exemple phase d'attaque ou phase de maintenue ...
				// en fonction de la valeur de cette variable, on va appeler une fonction permettant
				// de mettre à jour l'enveloppe courante.
	int Ecnt;	// Envelope counter = le compteur-enveloppe permet de savoir où l'on se trouve dans l'enveloppe
	int Einc;	// Envelope step courant
	int Ecmp;	// Envelope counter limite pour la prochaine phase
	int EincA;	// Envelope step for Attack = pas d'incrémentation du compteur durant la phase d'attaque
				// cette valeur est égal à AR[KSR]
	int EincD;	// Envelope step for Decay = pas d'incrémentation du compteur durant la phase de regression
				// cette valeur est égal à DR[KSR]
	int EincS;	// Envelope step for Sustain = pas d'incrémentation du compteur durant la phase de maintenue
				// cette valeur est égal à SR[KSR]
	int EincR;	// Envelope step for Release = pas d'incrémentation du compteur durant la phase de relâchement
				// cette valeur est égal à RR[KSR]
	int *OUTp;	// pointeur of SLOT output = pointeur permettant de connecter la sortie de ce slot à l'entrée
				// d'un autre ou carrement à la sortie de la voie
	int INd;	// input data of the slot = données en entrée du slot
	int ChgEnM;	// Change envelop mask.
	int AMS;	// AMS depth level of this SLOT = degré de modulation de l'amplitude par le LFO
	int AMSon;	// AMS enable flag = drapeau d'activation de l'AMS
} slot_;

typedef struct channel__ {
	int S0_OUT[4];			// anciennes sorties slot 0 (pour le feed back)
	int Old_OUTd;			// ancienne sortie de la voie (son brut)
	int OUTd;				// sortie de la voie (son brut)
	int LEFT;				// LEFT enable flag
	int RIGHT;				// RIGHT enable flag
	int ALGO;				// Algorythm = détermine les connections entre les opérateurs
	int FB;					// shift count of self feed back = degré de "Feed-Back" du SLOT 1 (il est son unique entrée)
	int FMS;				// Fréquency Modulation Sensitivity of channel = degré de modulation de la fréquence sur la voie par le LFO
	int AMS;				// Amplitude Modulation Sensitivity of channel = degré de modulation de l'amplitude sur la voie par le LFO
	int FNUM[4];			// hauteur fréquence de la voie (+ 3 pour le mode spécial)
	int FOCT[4];			// octave de la voie (+ 3 pour le mode spécial)
	int KC[4];				// Key Code = valeur fonction de la fréquence (voir KSR pour les slots, KSR = KC >> KSR_S)
	struct slot__ SLOT[4];	// four slot.operators = les 4 slots de la voie
	//int FFlag;				// Frequency step recalculation flag
	UINT8 Mute;				// Maxim: channel mute flag
} channel_;

struct ym2612__ {
	DEV_DATA _devData;

	int Clock;			// Horloge YM2612
	int Rate;			// Sample Rate (11025/22050/44100)
	int TimerBase;		// TimerBase calculation
	int Status;			// YM2612 Status (timer overflow)
	int OPNAadr;		// addresse pour l'écriture dans l'OPN A (propre à l'émulateur)
	int OPNBadr;		// addresse pour l'écriture dans l'OPN B (propre à l'émulateur)
	int LFOcnt;			// LFO counter = compteur-fréquence pour le LFO
	int LFOinc;			// LFO step counter = pas d'incrémentation du compteur-fréquence du LFO
						// plus le pas est grand, plus la fréquence est grande
	int TimerA;			// timerA limit = valeur jusqu'à laquelle le timer A doit compter
	int TimerAL;
	int TimerAcnt;		// timerA counter = valeur courante du Timer A
	int TimerB;			// timerB limit = valeur jusqu'à laquelle le timer B doit compter
	int TimerBL;
	int TimerBcnt;		// timerB counter = valeur courante du Timer B
	UINT8 Mode;			// Mode actuel des voie 3 et 6 (normal / spécial)
	UINT8 DAC;			// DAC enabled flag
	int DACdata;		// DAC data
	int dac_highpass;
	double Frequence;	// Fréquence de base, se calcul par rapport à l'horlage et au sample rate
	unsigned int Inter_Cnt;			// Interpolation Counter
	unsigned int Inter_Step;		// Interpolation Step
	struct channel__ CHANNEL[6];	// Les 6 voies du YM2612
	UINT8 REG[2][0x100];	// Sauvegardes des valeurs de tout les registres, c'est facultatif
							// cela nous rend le débuggage plus facile

	// --- tables ---
	unsigned int FINC_TAB[2048];    // Frequency step table

	unsigned int AR_TAB[128];       // Attack rate table
	unsigned int DR_TAB[96];        // Decay rate table
	signed int DT_TAB[8][32];       // Detune table

	int LFO_ENV_UP[MAX_UPDATE_LENGTH];      // Temporary calculated LFO AMS (adjusted for 11.8 dB)
	int LFO_FREQ_UP[MAX_UPDATE_LENGTH];     // Temporary calculated LFO FMS

	int in0, in1, in2, in3;         // current phase calculation
	int en0, en1, en2, en3;         // current enveloppe calculation

	int LFO_INC_TAB[8];             // LFO step table

	UINT8 DAC_Mute;

	/* Gens */
	UINT8 Enable_SSGEG; // enable SSG-EG envelope (causes inacurate sound sometimes - rodrigo)
	UINT8 DAC_Highpass_Enable; // sometimes it creates a terrible noise
	/* end */
};

// used for forward...
static void Update_Chan_Algo0(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo1(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo2(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo3(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo4(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo5(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo6(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo7(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);

static void Update_Chan_Algo0_LFO(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo1_LFO(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo2_LFO(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo3_LFO(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo4_LFO(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo5_LFO(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo6_LFO(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo7_LFO(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);

static void Update_Chan_Algo0_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo1_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo2_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo3_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo4_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo5_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo6_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo7_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);

static void Update_Chan_Algo0_LFO_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo1_LFO_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo2_LFO_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo3_LFO_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo4_LFO_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo5_LFO_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo6_LFO_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);
static void Update_Chan_Algo7_LFO_Int(ym2612_ *YM2612, channel_ *CH, DEV_SMPL **buf, UINT32 lenght);

static void Env_Attack_Next(ym2612_ *YM2612, slot_ *SL);
static void Env_Decay_Next(ym2612_ *YM2612, slot_ *SL);
static void Env_Substain_Next(ym2612_ *YM2612, slot_ *SL);
static void Env_Release_Next(ym2612_ *YM2612, slot_ *SL);
static void Env_NULL_Next(ym2612_ *YM2612, slot_ *SL);

#endif	// _YM2612_INT_H_
