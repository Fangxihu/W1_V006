#include "input_event_manager.h"

#include "1_button.h"

const InputEventConfig_t InputEventConfig  = 
{
	/* Table to convert from PIO to input event ID*/
	{
		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	},

	/* Masks for each PIO bank to configure as inputs */
	{ 0x00000001UL, 0x00000000UL, 0x00000000UL },
	/* PIO debounce settings */
	4, 5
};

const InputActionMessage_t InputEventActions[] = 
{
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		HELD,                                   /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_6,                  /* Message */
	},
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		HELD,                                   /* Action */
		2000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_2,                  /* Message */
	},
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_1,                  /* Message */
	},
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		HELD,                                   /* Action */
		7000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_7,                  /* Message */
	},
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		HELD_RELEASE,                           /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_6_SECOND,                /* Message */
	},
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		HELD_RELEASE,                           /* Action */
		2000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_2_SECOND,                /* Message */
	},
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		HELD_RELEASE,                           /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_1_SECOND,                /* Message */
	},
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		HELD_RELEASE,                           /* Action */
		7000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_7_SECOND,                /* Message */
	},
	{
		MFB_BUTTON,                             /* Input event bits */
		MFB_BUTTON,                             /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_PRESS,                   /* Message */
	},
};


