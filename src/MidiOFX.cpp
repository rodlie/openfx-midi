/*
* ------------------------------------------------------------------
* openfx-midi <https://github.com/rodlie/openfx-midi>
*
* Copyright (C) 2019 Ole-André Rodlie <ole.andre.rodlie@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
* ------------------------------------------------------------------
*/

#include "ofxsImageEffect.h"
#include "RtMidi.h"

#include <iostream>
#include <sstream>

#define kPluginName "MidiOFX"
#define kPluginGrouping "Other"
#define kPluginIdentifier "net.sf.openfx.Midi"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 0
#define kPluginDescription \
    "This is a meta node that can be used to get values " \
    "from MIDI controllers. This node can be expanded with PyPlugs " \
    "to provide many useful functions in Natron.\n\n" \
    "This node was written by Ole-André Rodlie and " \
    "licenced under the LGPL version 2.1 (or later). " \
    "This node uses RtMidi (https://github.com/thestk/rtmidi) " \
    "as a backend for cross-platform MIDI support. " \
    "The RtMidi license is similar to the MIT License, " \
    "with the added feature that modifications be sent to the developer."

#define kParamPort "port"
#define kParamPortLabel "Port"
#define kParamPortHint "MIDI input port (device)."

#define kParamMidiInput "input"
#define kParamMidiInputLabel "Input"
#define kParamMidiInputHint "The ID of the MIDI knob you want to get the value from."

#define kParamMidiInputValue "value"
#define kParamMidiInputValueLabel "Value"
#define kParamMidiInputValueHint "MIDI value."

// hardcoded MIDI min/max, should be added as params(?)
#define MIDI_MIN 0
#define MIDI_MAX 127

// params supported
// 25 should be enough for most needs (famous last words)
#define MIDI_PARAMS 25

std::string istring(unsigned i)
{
    std::stringstream ss;
    ss << i;
    return ss.str();
}

std::list<std::string> getDevices()
{
    std::list<std::string> result;
    RtMidiIn midi;
    unsigned int ports = midi.getPortCount();
    for (unsigned int i=0;i<ports;++i) {
        result.push_back(midi.getPortName(i));
    }
    return result;
}

using namespace OFX;

class MidiPlugin : public ImageEffect
{
public:
    MidiPlugin(OfxImageEffectHandle handle);
    virtual ~MidiPlugin();

    virtual void render(const RenderArguments &args) override final;
    virtual void changedParam(const InstanceChangedArgs &args,
                              const std::string &paramName) override final;
    void openInput(int port = -1);
    static void inputHandler(double deltatime,
                             std::vector< unsigned char > *message,
                             void *userData);
    void setInputValue(int key, int value);
    bool startsWith(const std::string str,
                    const std::string what);

private:
    // do not need to delete these, the ImageEffect is managing them for us
    Clip *_dstClip;
    RtMidiIn *_midiIn;
    ChoiceParam *_devices;
    std::vector<IntParam*> _paramsKey;
    std::vector<IntParam*> _paramsValue;
};

MidiPlugin::MidiPlugin(OfxImageEffectHandle handle)
: ImageEffect(handle)
, _dstClip(NULL)
, _midiIn(NULL)
, _devices(NULL)
, _paramsKey(MIDI_PARAMS,(IntParam*)NULL)
, _paramsValue(MIDI_PARAMS,(IntParam*)NULL)
{
    _devices = fetchChoiceParam(kParamPort);
    assert(_devices);

    for (unsigned i = 0; i < MIDI_PARAMS; ++i) {
        std::string nb = istring(i);
        _paramsKey[i] = fetchIntParam(kParamMidiInput + nb);
        _paramsValue[i] = fetchIntParam(kParamMidiInputValue + nb);
        assert(_paramsKey[i] && _paramsValue[i]);
    }

    _midiIn = new RtMidiIn();
    _midiIn->setCallback(&MidiPlugin::inputHandler, (void *)this);
    _midiIn->ignoreTypes(false, false, false);
    openInput();
}

MidiPlugin::~MidiPlugin()
{
    if (_midiIn->isPortOpen()) { _midiIn->closePort(); }
}

void MidiPlugin::render(const RenderArguments &args)
{
    // do we need to output an image? Seems to work without...
}

void MidiPlugin::changedParam(const InstanceChangedArgs &args,
                              const std::string &paramName)
{
    clearPersistentMessage();
    if (startsWith(paramName, kParamMidiInput)) {
        if (!_midiIn->isPortOpen()) {
            setPersistentMessage(Message::eMessageWarning, "", "MIDI input not connected");
        }
    } else if (paramName == kParamPort) {
        openInput();
    }
}

void MidiPlugin::openInput(int port)
{
    unsigned int ports = _midiIn->getPortCount();
    if (ports == 0) {
        setPersistentMessage(Message::eMessageWarning, "", "No MIDI input found");
        return;
    }
    if (_midiIn->isPortOpen()) { _midiIn->closePort(); }
    if (port == -1) { _devices->getValue(port); } // get port from knob
    std::cout << "MIDI OPEN port:" << port  << std::endl;
    _midiIn->openPort(port);
    if(!_midiIn->isPortOpen()) {
        setPersistentMessage(Message::eMessageWarning, "", "MIDI input not connected");
    }
}

void MidiPlugin::inputHandler(double deltatime,
                              std::vector<unsigned char> *message,
                              void *userData)
{
    if (message->size()<3) { return; }
    int key = (int)message->at(1);
    int value = (int)message->at(2);
    reinterpret_cast<MidiPlugin*>(userData)->setInputValue(key, value);
}

void MidiPlugin::setInputValue(int key, int value)
{
    std::cout << "MIDI INPUT key:" << key << " value: " << value << std::endl;
    for (unsigned i = 0; i < MIDI_PARAMS; ++i) {
        int hasKey;
        _paramsKey[i]->getValue(hasKey);
        if (hasKey == key) {
            _paramsValue[i]->setValue(value);
        }
    }
}

bool MidiPlugin::startsWith(const std::string str,
                            const std::string what)
{
    return str.rfind(what, 0) == 0;
}

mDeclarePluginFactory(MidiPluginFactory, {}, {});

void MidiPluginFactory::describe(ImageEffectDescriptor &desc)
{
    desc.setLabel(kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);
    desc.addSupportedContext(eContextGenerator);
    desc.addSupportedBitDepth(eBitDepthFloat);
    desc.setSupportsTiles(0);
    desc.setSupportsMultiResolution(0);
    desc.setRenderThreadSafety(eRenderFullySafe);
}

void MidiPluginFactory::describeInContext(ImageEffectDescriptor &desc,
                                          ContextEnum /*context*/)
{
    ClipDescriptor* srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->setOptional(true);

    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->setSupportsTiles(0);

    PageParamDescriptor *page = desc.definePageParam("Controls");
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamPort);
        param->setLabel(kParamPortLabel);
        param->setHint(kParamPortHint);
        param->setAnimates(false);
        param->setLayoutHint(eLayoutHintDivider);

        std::list<std::string>::const_iterator device;
        std::list<std::string> devices = getDevices();
        for(device = devices.begin(); device != devices.end(); ++device) {
            std::string deviceName = *device;
            if (!deviceName.empty()) {
                param->appendOption(deviceName);
            }
        }
        if (page) {
            page->addChild(*param);
        }
    }
    for (unsigned i = 0; i < MIDI_PARAMS; ++i) {
        std::string nb = istring(i);
        {
            IntParamDescriptor* param = desc.defineIntParam(kParamMidiInput + nb);
            param->setLabel(kParamMidiInputLabel);
            param->setHint(kParamMidiInputHint);
            param->setRange(MIDI_MIN, MIDI_MAX);
            param->setDisplayRange(MIDI_MIN, MIDI_MIN);
            param->setDefault(MIDI_MIN);
            param->setAnimates(false);
            param->setEvaluateOnChange(true);
            param->setLayoutHint(eLayoutHintNoNewLine, 1);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            IntParamDescriptor* param = desc.defineIntParam(kParamMidiInputValue + nb);
            param->setLabel(kParamMidiInputValueLabel);
            param->setHint(kParamMidiInputValueHint);
            param->setRange(MIDI_MIN, MIDI_MAX);
            param->setDefault(MIDI_MIN);
            param->setAnimates(false);
            param->setEvaluateOnChange(true);
            param->setLayoutHint(eLayoutHintDivider);
            if (page) {
                page->addChild(*param);
            }
        }
    }
}

ImageEffect* MidiPluginFactory::createInstance(OfxImageEffectHandle handle,
                                               ContextEnum /*context*/)
{
    return new MidiPlugin(handle);
}

static MidiPluginFactory p(kPluginIdentifier,
                           kPluginVersionMajor,
                           kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
