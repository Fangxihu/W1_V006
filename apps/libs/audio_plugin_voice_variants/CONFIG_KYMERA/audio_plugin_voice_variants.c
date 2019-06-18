/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_plugin_voice_variants.c

DESCRIPTION
    Definitions of voice plug-in variants.
*/

#include <stdlib.h>
#include <print.h>

#include "audio_plugin_voice_variants.h"

/* HFP extern declarations */
extern CvcPluginTaskdata csr_cvsd_cvc_1mic_headset_plugin;
extern CvcPluginTaskdata csr_wbs_cvc_1mic_headset_plugin;
extern CvcPluginTaskdata csr_cvsd_cvc_2mic_headset_plugin;
extern CvcPluginTaskdata csr_wbs_cvc_2mic_headset_plugin;
extern CvcPluginTaskdata csr_cvsd_cvc_2mic_headset_binaural_plugin;
extern CvcPluginTaskdata csr_wbs_cvc_2mic_headset_binaural_plugin;

extern CvcPluginTaskdata csr_cvsd_cvc_1mic_speaker_plugin;
extern CvcPluginTaskdata csr_wbs_cvc_1mic_speaker_plugin;
extern CvcPluginTaskdata csr_cvsd_cvc_2mic_speaker_plugin;
extern CvcPluginTaskdata csr_wbs_cvc_2mic_speaker_plugin;

/* USB extern declarations */
extern CvcPluginTaskdata usb_nb_cvc_1mic_headset_plugin;
extern CvcPluginTaskdata usb_wb_cvc_1mic_headset_plugin;
extern CvcPluginTaskdata usb_nb_cvc_2mic_headset_plugin;
extern CvcPluginTaskdata usb_wb_cvc_2mic_headset_plugin;
extern CvcPluginTaskdata usb_nb_cvc_2mic_headset_binaural_plugin;
extern CvcPluginTaskdata usb_wb_cvc_2mic_headset_binaural_plugin;

extern CvcPluginTaskdata usb_wb_cvc_1mic_speaker_plugin;
extern CvcPluginTaskdata usb_nb_cvc_1mic_speaker_plugin;
extern CvcPluginTaskdata usb_nb_cvc_2mic_speaker_plugin;
extern CvcPluginTaskdata usb_wb_cvc_2mic_speaker_plugin;


/* HFP Headset plugins */
#define CVCHS1MIC       (TaskData *)&csr_cvsd_cvc_1mic_headset_plugin
#define CVCHS1MICWBS    (TaskData *)&csr_wbs_cvc_1mic_headset_plugin
#define CVCHS2MIC       (TaskData *)&csr_cvsd_cvc_2mic_headset_plugin
#define CVCHS2MICWBS    (TaskData *)&csr_wbs_cvc_2mic_headset_plugin
#define CVCHS2MICBINAURAL  (TaskData *)&csr_cvsd_cvc_2mic_headset_binaural_plugin
#define CVCHS2MICBINAURALWBS  (TaskData *)&csr_wbs_cvc_2mic_headset_binaural_plugin

/* HFP Speaker plugins */
#define CVCSPKR1MIC       (TaskData *)&csr_cvsd_cvc_1mic_speaker_plugin
#define CVCSPKR1MICWBS    (TaskData *)&csr_wbs_cvc_1mic_speaker_plugin
#define CVCSPKR2MIC       (TaskData *)&csr_cvsd_cvc_2mic_speaker_plugin
#define CVCSPKR2MICWBS    (TaskData *)&csr_wbs_cvc_2mic_speaker_plugin

/* USB Headset plugins */
#define USB_CVCHS1MIC       (TaskData *)&usb_nb_cvc_1mic_headset_plugin
#define USB_CVCHS1MICWBS    (TaskData *)&usb_wb_cvc_1mic_headset_plugin
#define USB_CVCHS2MIC       (TaskData *)&usb_nb_cvc_2mic_headset_plugin
#define USB_CVCHS2MICWBS    (TaskData *)&usb_wb_cvc_2mic_headset_plugin
#define USB_CVCHS2MICBINAURAL  (TaskData *)&usb_nb_cvc_2mic_headset_binaural_plugin
#define USB_CVCHS2MICBINAURALWBS   (TaskData *)&usb_wb_cvc_2mic_headset_binaural_plugin

/* USB Speaker plugins */
#define USB_CVCSPKR1MIC       (TaskData *)&usb_nb_cvc_1mic_speaker_plugin
#define USB_CVCSPKR1MICWBS    (TaskData *)&usb_wb_cvc_1mic_speaker_plugin
#define USB_CVCSPKR2MIC       (TaskData *)&usb_nb_cvc_2mic_speaker_plugin
#define USB_CVCSPKR2MICWBS    (TaskData *)&usb_wb_cvc_2mic_speaker_plugin

/* the column to use is selected by user PSKEY
   the row depends upon the audio link (codec) negotiated */
#define MAX_NUM_HFP_PLUGINS_PER_CODEC (max_num_voice_variants)

TaskData * const hfp_cvc_plugins[][MAX_NUM_HFP_PLUGINS_PER_CODEC] =
{
    /*CVSD*/
    {
      NULL,
      /*Headset plugins*/
      CVCHS1MIC,
      NULL, 
      CVCHS2MIC,
      NULL,
      CVCHS2MICBINAURAL,
      /*Handsfree plugins*/
      NULL,
      NULL,
      NULL,
      NULL,
      /*Speaker plugins*/
      CVCSPKR1MIC,
      CVCSPKR2MIC
    },
    /*MSBC*/
    {
      NULL,  
      /*Headset plugins*/
      CVCHS1MICWBS,
      NULL,
      CVCHS2MICWBS,
      NULL,
      CVCHS2MICBINAURALWBS,
      /*Handsfree plugins*/
      NULL,
      NULL,
      NULL,
      NULL,
      /*Speaker plugins*/
      CVCSPKR1MICWBS,
      CVCSPKR2MICWBS
     }
};

/* the column to use is selected by user PSKEY
   the row depends upon the audio link (codec) negotiated */
#define MAX_NUM_USB_PLUGINS_PER_CODEC (max_num_voice_variants)

TaskData * const usb_cvc_plugins[][MAX_NUM_USB_PLUGINS_PER_CODEC] =
{ 
    /*8kHz*/
    {
      NULL,
      /*Headset plugins*/
      USB_CVCHS1MIC,
      NULL,
      USB_CVCHS2MIC,
      NULL,
      USB_CVCHS2MICBINAURAL,
      /*Handsfree plugins*/
      NULL,
      NULL,  
      NULL,
      NULL,
      /*Speaker plugins*/
      USB_CVCSPKR1MIC,
      USB_CVCSPKR2MIC 
     },
     /*16kHz*/
     {
       NULL,  
       /*Headset plugins*/
       USB_CVCHS1MICWBS, 
       NULL,  
       USB_CVCHS2MICWBS,   
       NULL, 
       USB_CVCHS2MICBINAURALWBS,
       /*Handsfree plugins*/
       NULL, 
       NULL,   
       NULL,   
       NULL,
       /*Speaker plugins*/
       USB_CVCSPKR1MICWBS,
       USB_CVCSPKR2MICWBS
     }
};

Task AudioPluginVoiceVariantsGetHfpPlugin(hfp_wbs_codec_mask codec, plugin_index_t index)
{
    if((codec == hfp_wbs_codec_mask_cvsd) || (codec == hfp_wbs_codec_mask_msbc))
    {
        if(index < MAX_NUM_HFP_PLUGINS_PER_CODEC)
        {
            PRINT(("Voice: plugin [%p] \n" , (void*)plugin));
            return hfp_cvc_plugins[codec - hfp_wbs_codec_mask_cvsd][index];
        }
    }

    return NULL;
}

Task AudioPluginVoiceVariantsGetUsbPlugin(usb_voice_plugin_type codec, plugin_index_t index)
{
    if((codec == usb_voice_mono_nb) || (codec == usb_voice_mono_wb))
    {
        if(index < MAX_NUM_USB_PLUGINS_PER_CODEC)
        {
            PRINT(("Voice: plugin [%p] \n" , (void*)plugin));
            return usb_cvc_plugins[codec - usb_voice_mono_nb][index];
        }
    }

    return NULL;
}

Task AudioPluginVoiceVariantsGetAsrPlugin(hfp_wbs_codec_mask codec, plugin_index_t index)
{
    UNUSED(codec);
    UNUSED(index);
    return NULL;
}
