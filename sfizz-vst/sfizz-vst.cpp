// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstaudioprocessoralgo.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h" // for UString128
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstpresetkeys.h" // for use of IStreamAttributes

// #include "vstgui/plugin-bindings/vst3editor.h"

#include "base/source/fstreamer.h"
#include <math.h>
#include <stdio.h>

#include "absl/strings/str_format.h"
#include "sfizz-vst.h"

// this allows to enable the communication example between again and its
// controller

// using namespace VSTGUI;

using namespace Steinberg;
using namespace Steinberg::Vst;

Sfizz::Sfizz()
{
    DBG("Constructed plugin");
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::initialize(FUnknown* context)
{
    DBG("Entered initialization");
    tresult result = SingleComponentEffect::initialize(context);
    if (result != kResultOk)
        return result;

    //---create Audio In/Out buses------
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

    //---create Event In/Out buses (1 bus with only 1 channel)------
    addEventInput(STR16("Event In"), 1);

    //---Bypass parameter---
    auto flags = ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass;
    Vst::ParamID paramIdx { 0 };
    parameters.addParameter(USTRING("Bypass"), nullptr, 1, 0, flags, paramIdx);
    paramIdx++;
    // Add the MIDI parameter mappings
    for (int16 channelIdx = 0; channelIdx < numMidiChannels; channelIdx++) {
        for (CtrlNumber ccIdx = 0; ccIdx < Vst::kCountCtrlNumber; ccIdx++) {
            midiToParameterMapping[channelIdx][ccIdx] = paramIdx;
            parameterToChannelMapping[paramIdx] = channelIdx;
            parameterToCCMapping[paramIdx] = ccIdx;
            std::string paramName = absl::StrFormat("Midi CC %d/%d", channelIdx + 1, ccIdx);
            parameters.addParameter(UString128(paramName.c_str()), nullptr, 0, 0, 0, paramIdx);
            paramIdx++;
        }
    }

    // Load the file (temporary)
    synth.loadSfzFile({ "/home/paul/Documents/Sfz Instruments/SMDrums_Sforzando_1.2/Programs/SM_Drums_kit.sfz" });
    // synth.loadSfzFile({ "/home/paul/Documents/Sfz Instruments/AVL_Drumkits_1.1-fix/Black_Pearl_5pc.sfz" });

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::terminate()
{
    return SingleComponentEffect::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::setActive(TBool state)
{
    return kResultOk;
}

void Sfizz::dispatchParameterChanges(IParameterChanges* paramChanges)
{
    if (paramChanges == nullptr)
        return;

    auto numParamsChanged = paramChanges->getParameterCount();
    // for each parameter which are some changes in this audio block:
    for (auto i = 0; i < numParamsChanged; i++) {
        IParamValueQueue* paramQueue = paramChanges->getParameterData(i);
        if (paramQueue == nullptr)
            continue;

        ParamValue value;
        int32 sampleOffset;
        const int32 numPoints = paramQueue->getPointCount();
        const auto paramId = paramQueue->getParameterId();
        switch (paramId) {
        case 0:
            if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue) {
                bypass = (value > 0.5f);
            }
            break;
        default:
            if (paramId >= parameterToCCMapping.size())
                continue;

            auto channel = parameterToChannelMapping[paramId] + 1;
            auto ccIdx = parameterToCCMapping[paramId];

            if (ccIdx > 127) // This is not a MIDI event that we forward to the plugin
                continue;

            for (int32 pointIdx = 0; pointIdx < numPoints; pointIdx++) {
                if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) != kResultTrue) {
                    const auto ccValue = static_cast<uint8_t>(value * 127);
                    synth.cc(sampleOffset, channel, ccIdx, ccValue);
                }
            }
        }
    }
}

void Sfizz::dispatchEvents(IEventList* eventList)
{
    if (eventList == nullptr)
        return;

    auto numEvent = eventList->getEventCount();
    for (auto i = 0; i < numEvent; i++) {
        Event event;
        if (eventList->getEvent(i, event) != kResultOk)
            continue;

        if (event.type != Event::kNoteOnEvent && event.type != Event::kNoteOffEvent)
            continue;

        const auto velocity = static_cast<uint8_t>(event.noteOn.velocity * 127);
        switch (event.type) {
        case Event::kNoteOnEvent:
            if (velocity > 0)
                synth.noteOn(event.sampleOffset, event.noteOn.channel + 1, event.noteOn.pitch, velocity);
            else // Seems that for some midi tracks note offs are actually note ons with zero velocity...
                synth.noteOff(event.sampleOffset, event.noteOn.channel + 1, event.noteOn.pitch, 0);
            break;
        case Event::kNoteOffEvent:
            synth.noteOff(event.sampleOffset, event.noteOff.channel + 1, event.noteOff.pitch, 0);
            break;
        }
    }
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::process(ProcessData& data)
{
    // finally the process function
    // In this example there are 4 steps:
    // 1) Read inputs parameters coming from host (in order to adapt our model
    // values) 2) Read inputs events coming from host (we apply a gain reduction
    // depending of the velocity of pressed key) 3) Process the gain of the input
    // buffer to the output buffer 4) Write the new VUmeter value to the output
    // Parameters queue

    //---1) Read inputs parameter changes-----------
    dispatchParameterChanges(data.inputParameterChanges);
    dispatchEvents(data.inputEvents);

    //-------------------------------------
    //---3) Process Audio---------------------
    //-------------------------------------
    if (data.numOutputs == 0) // nothing to do
        return kResultOk;

    //---get audio buffers----------------
    auto numChannels = data.outputs[0].numChannels;
    void** out = getChannelBuffersPointer(processSetup, data.outputs[0]);
    AudioSpan<float> outputs { { reinterpret_cast<float*>(out[0]), reinterpret_cast<float*>(out[1]) }, static_cast<unsigned>(data.numSamples) };
    // mark our outputs as not silent
    data.outputs[0].silenceFlags = 0;
    for (int32 i = 0; i < numChannels; i++)
        memset(out[i], 0, data.numSamples * sizeof(float));

    if (bypass)
        return kResultOk;

    synth.renderBlock(outputs);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::setState(IBStream* state)
{
    // we receive the current  (processor part)
    // called when we load a preset, the model has to be reloaded

    IBStreamer streamer(state, kLittleEndian);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::getState(IBStream* state)
{
    // here we need to save the model

    IBStreamer streamer(state, kLittleEndian);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::setupProcessing(ProcessSetup& newSetup)
{
    // called before the process call, always in a disable state (not active)
    // here we keep a trace of the processing mode (offline,...) for example.
    currentProcessMode = newSetup.processMode;
    synth.setSamplesPerBlock(newSetup.maxSamplesPerBlock);
    synth.setSampleRate(newSetup.sampleRate);
    return SingleComponentEffect::setupProcessing(newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::setBusArrangements(SpeakerArrangement* inputs,
    int32 numIns,
    SpeakerArrangement* outputs,
    int32 numOuts)
{
    if (numIns == 0 && numOuts == 2 && SpeakerArr::getChannelCount(outputs[0]) == 2) {
        removeAudioBusses();
        addAudioOutput(STR16("Stereo Out"), outputs[0]);
        return kResultOk;
    }
    return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::canProcessSampleSize(int32 symbolicSampleSize)
{
    if (symbolicSampleSize == kSample32)
        return kResultTrue;

    return kResultFalse;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API Sfizz::createView(const char* name)
{
    // someone wants my editor
    // if (name && strcmp(name, ViewType::kEditor) == 0) {
    //   auto *view = new VST3Editor(this, "view", "again.uidesc");
    //   return view;
    // }
    return nullptr;
}

//------------------------------------------------------------------------
VSTGUI::IController* Sfizz::createSubController(UTF8StringPtr name,
    const IUIDescription* description,
    VST3Editor* editor)
{
    // if (UTF8StringView(name) == "MessageController") {
    //   auto *controller = new UIMessageController(this);
    //   addUIMessageController(controller);
    //   return controller;
    // }
    return nullptr;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::setEditorState(IBStream* state)
{
    tresult result = kResultFalse;

    // int8 byteOrder;
    // if ((result = state->read(&byteOrder, sizeof(int8))) != kResultTrue)
    //   return result;
    // if ((result = state->read(defaultMessageText, 128 * sizeof(TChar))) !=
    //     kResultTrue)
    //   return result;

    // // if the byteorder doesn't match, byte swap the text array ...
    // if (byteOrder != BYTEORDER) {
    //   for (int32 i = 0; i < 128; i++)
    //     SWAP_16(defaultMessageText[i])
    // }

    // for (auto &uiMessageController : uiMessageControllers)
    //   uiMessageController->setMessageText(defaultMessageText);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::getEditorState(IBStream* state)
{
    // here we can save UI settings for example

    IBStreamer streamer(state, kLittleEndian);

    // // as we save a Unicode string, we must know the byteorder when setState is
    // // called
    // int8 byteOrder = BYTEORDER;
    // if (streamer.writeInt8(byteOrder) == false)
    //   return kResultFalse;

    // if (streamer.writeRaw(defaultMessageText, 128 * sizeof(TChar)) == false)
    //   return kResultFalse;

    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::setParamNormalized(ParamID tag, ParamValue value)
{
    // called from host to update our parameters state
    tresult result = SingleComponentEffect::setParamNormalized(tag, value);
    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::getParamStringByValue(ParamID tag,
    ParamValue valueNormalized,
    String128 string)
{
    return SingleComponentEffect::getParamStringByValue(tag, valueNormalized,
        string);
}

//------------------------------------------------------------------------
tresult PLUGIN_API Sfizz::getParamValueByString(ParamID tag, TChar* string,
    ParamValue& valueNormalized)
{
    return SingleComponentEffect::getParamValueByString(tag, string,
        valueNormalized);
}

//------------------------------------------------------------------------
enum {
    // UI size
    kEditorWidth = 350,
    kEditorHeight = 120
};

//------------------------------------------------------------------------
// called when library is loaded
bool InitModule() { return true; }

//------------------------------------------------------------------------
// called when library is unloaded
bool DeinitModule() { return true; }

//------------------------------------------------------------------------
BEGIN_FACTORY_DEF("SFZTools",
    "http://sfztools.github.io",
    "mailto:paulfd@outlook.fr")

//---First Plug-in included in this factory-------
DEF_CLASS2(INLINE_UID(0xa09cd0c9, 0x9bd049fa, 0x8f6e87e2, 0x9819d9a2),
    PClassInfo::kManyInstances, // cardinality
    kVstAudioEffectClass, // the component category (do not changed this)
    "Sfizz", // here the Plug-in name (to be changed)
    0, // single component effects can not be distributed so this is zero
    "VSTi", // Subcategory for this Plug-in (to be changed)
    FULL_VERSION_STR, // Plug-in version (to be changed)
    kVstVersionString, // the VST 3 SDK version (do not changed this, use
    // always this define)
    Steinberg::Vst::Sfizz::createInstance) // function pointer called
// when this component
// should be instantiated

END_FACTORY
