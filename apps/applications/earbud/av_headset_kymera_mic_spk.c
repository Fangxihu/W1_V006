/*!
\copyright  Copyright (c) 2017-2018  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.2
\file
\brief      Kymera tones / prompts
*/
#ifdef CHAIN_MIC_SPK
#include "av_headset_kymera_private.h"


#include "chains/chain_prompt_decoder.h"
#include "chains/chain_prompt_decoder_no_iir.h"
#include "chains/chain_prompt_pcm.h"
#include "chains/chain_loopback.h"
#if 0
/*! \brief Create the tone / prompt audio chain.
    \param msg Message containing the create parameters.
*/
static Source appKymeraCreateMicPromptChain(const KYMERA_INTERNAL_TONE_PROMPT_PLAY_T *msg)
{
    kymeraTaskData *theKymera = appGetKymera();
    kymera_chain_handle_t loop_chain = NULL;
    const chain_config_t *config = NULL;
    const bool has_resampler = (theKymera->output_rate != msg->rate);
    const bool is_prompt = (msg->prompt != FILE_NONE);

    /* Can play tone or prompt, not both */
            config = chain_loopback_config;

    if (config)
    {
        Operator op;
        loop_chain = ChainCreate(config);
        if (has_resampler)
        {
            /* Configure resampler */
            op = ChainGetOperatorByRole(chain, OPR_TONE_PROMPT_RESAMPLER);
            OperatorsResamplerSetConversionRate(op, msg->rate, theKymera->output_rate);
        }
        ChainConnect(chain);
        theKymera->chain_tone_handle = chain;
    }

    return ChainGetOutput(chain, EPR_TONE_PROMPT_CHAIN_OUT);
}
#endif
#if 0
void appKymerStartLoopback(void)
{
    if (appKymeraGetState() == KYMERA_STATE_TONE_PLAYING)   
	{      /* If there is a tone still playing at this point, 
        	* it must be an interruptable tone, so cut it off 
            */        
	appKymeraTonePromptStop();    
	}
    /* Configure DSP power mode appropriately for CVC */
	//    appKymeraConfigureDspPowerMode(FALSE);
    /* Create appropriate loopback chain */   
	kymera_chain_handle_t loop_chain = ChainCreate(&chain_loopback_config);
	//set volume
	appKymeraSetMainVolume(loop_chain, 150);
    /* Get microphone sources */    
	Source mic_src_1a;    
	Source mic_src_1b;   
    //appKymeraMicSetup(appConfigScoMic1(), &mic_src_1a, appConfigScoMic2(), &mic_src_1b, 8000);

    /* Get speaker sink */   
	Sink spk_snk = StreamAudioSink(appConfigLeftAudioHardware(), appConfigLeftAudioInstance(), appConfigLeftAudioChannel());    
	SinkConfigure(spk_snk, STREAM_CODEC_OUTPUT_RATE, 8000);
    /* Get sources and sinks for chain endpoints */   
	Source spk_src  = ChainGetOutput(loop_chain, EPR_SOURCE_MIXER_OUT);    
	Sink mic_snk    = ChainGetInput(loop_chain, EPR_SCO_MIC1);
    /* Connect mic and speake to chain endpoints */   
        StreamConnect(mic_src_1a, mic_snk);
        StreamConnect(spk_src, spk_snk);
    /* Connect chain */    
	ChainConnect(loop_chain);
    /* May need to exit low power mode to play tone simultaneously */
	//    appKymeraConfigureDspPowerMode(TRUE);
    ChainStart(loop_chain);
}
#endif
/************************************************************
                        开启MIC-> SPK通道
*************************************************************/
void appKymerLoopbackStart(void)
{    
    //DEBUG_LOGF("=================Start=========================");
	kymeraTaskData *theKymera = appGetKymera();
    if (appKymeraGetState() == KYMERA_STATE_TONE_PLAYING)  
	{        /* If there is a tone still playing at this point,        
	* it must be an interruptable tone, so cut it off */      
	appKymeraTonePromptStop();   
	}

	/* Move to SCO active state now, what ever happens we end up in this state
      (even if it's temporary) */
    /* Create appropriate loopback chain */   
	kymera_chain_handle_t loop_chain = ChainCreate(&chain_loopback_config);   
	theKymera->loopback_handle = loop_chain;
	
    //Set volume    
        appKymeraSetMainVolume(loop_chain, 127);
        //appKymeraSetVolume(loop_chain, 127);
    /* Get microphone sources */     
	//采样率不能超过16K
	Source mic_src_1a = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_A);
    Sink mic_snk = ChainGetInput(loop_chain, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Source mic_src_1b = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_B);
    Sink mic_sink1b = ChainGetInput(scoChain, EPR_SCO_MIC2);
#endif


    /* Get speaker sink */   
	Sink spk_snk = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());    

    /* Get sources and sinks for chain endpoints */   
	Source spk_src  = ChainGetOutput(loop_chain, EPR_SOURCE_MIXER_OUT);    

/* Set DAC and ADC sample rate */
    SourceConfigure(mic_src_1a, STREAM_CODEC_INPUT_RATE, 8000);
    SourceConfigure(mic_src_1a, STREAM_CODEC_INPUT_GAIN, appConfigMicGain());
#ifdef HFP_USE_2MIC
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_RATE, 8000);
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_GAIN, appConfigMicGain());
    SourceSynchronise(mic_src1,mic_src1b);
#endif
	SinkConfigure(spk_snk, STREAM_CODEC_OUTPUT_RATE, 8000);
    /* Connect mic and speake to chain endpoints */   
        StreamConnect(mic_src_1a, mic_snk);
        StreamConnect(spk_src, spk_snk);
    /* Connect chain */   
	ChainConnect(loop_chain);
    /* Turn on MIC bias */
        MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_FORCE_ON);
    ChainStart(loop_chain);
}

/************************************************************
                        关闭MIC-> SPK通道
*************************************************************/
void appKymerLoopbackStop(void)
{    
	appScoFwdLoopbackStop();

    //DEBUG_LOGF("===============Stop==========================");
    kymeraTaskData *theKymera = appGetKymera();

	kymera_chain_handle_t loop_chain = theKymera->loopback_handle;    
	/* A tone still playing at this point must be interruptable */    
    //appKymeraTonePromptStop();   //开启tone 时不会影响好mic->spk 的通道
    /* Stop chains before disconnecting */   
	ChainStop(loop_chain);
    /* Get sources and sinks for chain endpoints */   
	Source spk_src  = ChainGetOutput(loop_chain, EPR_SOURCE_MIXER_OUT);    
	Sink mic_snk    = ChainGetInput(loop_chain, EPR_SCO_MIC1);
    /* Disconnect chain speaker endpoint to speaker */   
	StreamDisconnect(spk_src, NULL);    
	/* Disconnect microphones from chain microphone endpoints */   
	StreamDisconnect(NULL, mic_snk);
    /* Close microphone sources */  
    //appKymeraMicCleanup(appConfigScoMic1(), appConfigScoMic2());

    /* Turn off MIC bias */
        /*! \todo Check if MIC_BIAS different for 2 mic */
        MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_OFF);

        /* Update state variables */
        theKymera->state = KYMERA_STATE_IDLE;
        theKymera->output_rate = 0;
    /* Destroy chains now that input has been disconnected */   
	ChainDestroy(loop_chain);
}
#endif
