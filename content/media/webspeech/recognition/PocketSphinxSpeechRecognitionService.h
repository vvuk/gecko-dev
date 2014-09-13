/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PocketSphinxRecognitionService_h
#define mozilla_dom_PocketSphinxRecognitionService_h

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "speex/speex_resampler.h"
#include <vector>

extern "C"
{
  #include <pocketsphinx/pocketsphinx.h>
  #include <sphinxbase/sphinx_config.h>
}

#include "nsISpeechRecognitionService.h"

#define NS_POCKETSPHINX_SPEECH_RECOGNITION_SERVICE_CID \
{ 0x0ff5ce56, 0x5b09, 0x4db8, { 0xad, 0xc6, 0x82, 0x66, 0xaf, 0x95, 0xf8, 0x64 } };

namespace mozilla {

class PocketSphinxSpeechRecognitionService : public nsISpeechRecognitionService,
                                     public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISPEECHRECOGNITIONSERVICE
  NS_DECL_NSIOBSERVER

  PocketSphinxSpeechRecognitionService();

private:
  virtual ~PocketSphinxSpeechRecognitionService();
  bool IslastWasWS; 
  WeakPtr<dom::SpeechRecognition> mRecognition;
  dom::SpeechRecognitionResultList* BuildMockResultList();
  SpeexResamplerState *mSpeexState;
  ps_decoder_t * mPSHandle;
  cmd_ln_t *mPSConfig;
  bool ISDecoderCreated; // flag to verify if decoder was created
  bool ISGrammarCompiled; // flag to verify if grammar was compiled
  std::vector<int16_t> mAudioVector;
};


} // namespace mozilla

#endif
