enum {
	PinLed = 2,
	PinButton = 4,
	PinPot1 = 17,
	PinPot2 = 18,
	PinSaleaIsr = 12,
	PinSaleaIsrEdge = 3, /* OCR2A output */
	PinSaleaUser = 7,
	PinSaleaSerial = 8,
	PinSaleaError = 9,
	PinTrigOut = 19,
	PinBCK = 13,
	PinDIN = 11,
	PinWS = 10,
	PinAudio = -1, /* determined by timer configuration */
};

#define TimerClock 0
#define TimerAudio 2
