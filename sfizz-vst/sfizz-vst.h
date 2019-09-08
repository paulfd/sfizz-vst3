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

#pragma once

// must always come first
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
//------------------------------------------------------------------------
#include "Synth.h"
#include "version.h" 

// GUI stuff
#include "public.sdk/source/vst/vstguieditor.h"
#include "pluginterfaces/vst/ivstcontextmenu.h"
#include "pluginterfaces/vst/ivstplugview.h"

#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui4/vstgui/lib/vstguibase.h"
#include "vstgui4/vstgui/plugin-bindings/vst3editor.h"
#include "vstgui4/vstgui/uidescription/icontroller.h"

namespace Steinberg {
namespace Vst {

// template <typename T>
// class AGainUIMessageController;

//------------------------------------------------------------------------
// AGain as combined processor and controller
//------------------------------------------------------------------------
class Sfizz : public SingleComponentEffect, public VSTGUI::VST3EditorDelegate, public IMidiMapping
{
public:
//------------------------------------------------------------------------
	// using UIMessageController = AGainUIMessageController<Sfizz>;
	using UTF8StringPtr = VSTGUI::UTF8StringPtr;
	using IUIDescription = VSTGUI::IUIDescription;
	using IController = VSTGUI::IController;
	using VST3Editor = VSTGUI::VST3Editor;

	Sfizz ();

	static FUnknown* createInstance (void* context)
	{
		return (IAudioProcessor*)new Sfizz;
	}

	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE;
	tresult PLUGIN_API process (ProcessData& data) SMTG_OVERRIDE;
	tresult PLUGIN_API canProcessSampleSize (int32 symbolicSampleSize) SMTG_OVERRIDE;
	tresult PLUGIN_API setState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API getState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API setupProcessing (ProcessSetup& newSetup) SMTG_OVERRIDE;
	tresult PLUGIN_API setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
	                                       SpeakerArrangement* outputs,
	                                       int32 numOuts) SMTG_OVERRIDE;

	IPlugView* PLUGIN_API createView (const char* name) SMTG_OVERRIDE;
	tresult PLUGIN_API setEditorState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API getEditorState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API setParamNormalized (ParamID tag, ParamValue value) SMTG_OVERRIDE;
	tresult PLUGIN_API getParamStringByValue (ParamID tag, ParamValue valueNormalized,
	                                          String128 string) SMTG_OVERRIDE;
	tresult PLUGIN_API getParamValueByString (ParamID tag, TChar* string,
	                                          ParamValue& valueNormalized) SMTG_OVERRIDE;

	//---from VST3EditorDelegate-----------
	IController* createSubController (UTF8StringPtr name, const IUIDescription* description,
	                                  VST3Editor* editor) SMTG_OVERRIDE;

	// From IMidiMapping
	tresult getMidiControllerAssignment (int32 busIndex [[maybe_unused]], int16 channel, CtrlNumber midiControllerNumber, ParamID &id) SMTG_OVERRIDE
	{
		id = midiToParameterMapping[channel][midiControllerNumber];
        return kResultTrue;
	}
	tresult PLUGIN_API queryInterface (const TUID iid, void** obj) SMTG_OVERRIDE
	{
		DEF_INTERFACE (IMidiMapping);
		return SingleComponentEffect::queryInterface(iid, obj);
	}
	REFCOUNT_METHODS (EditControllerEx1)
//------------------------------------------------------------------------
private:
	void dispatchParameterChanges(IParameterChanges* paramChanges);
	void dispatchEvents(IEventList* eventList);
	static constexpr int16 numMidiChannels { 16 };
	sfz::Synth synth;
	Vst::ParamID midiToParameterMapping[numMidiChannels][Vst::kCountCtrlNumber];
	std::array<int16, numMidiChannels * Vst::kCountCtrlNumber> parameterToChannelMapping;
	std::array<CtrlNumber, numMidiChannels * Vst::kCountCtrlNumber> parameterToCCMapping;
	int32 currentProcessMode { -1 };
	bool bypass { false };
};

//------------------------------------------------------------------------
} // namespace Vst
} // namespace Steinberg
