#include <RtMidi.h>
//#include <rtmidi/RtMidi.h>
#include <iostream>
#include <cstdlib>
#include <queue>
#include <inttypes.h>
#include <string.h>

#include "midilib.h"

struct MidiDevice
{
    int inConnected;
    int outConnected;
    std::string name;
    std::queue<std::vector<unsigned char> > queue;
    RtMidiIn *midiin;
    RtMidiOut *midiout;
};

static void mycallback( double deltatime, std::vector< unsigned char > *message, void *_dev)
{
    MidiDevice *dev = (MidiDevice *) _dev;
    dev->queue.push (*message);
}

static bool chooseMidiInPort(RtMidiIn *rtmidi, std::string name)
{
    unsigned int nPorts = rtmidi->getPortCount ();
    for (unsigned int i=0; i<nPorts; i++ ) {
        std::string portName = rtmidi->getPortName (i);
        std::cout << "chooseMidiInPort:  found '" << portName << "'\n";
        if (strncmp (portName.c_str(), name.c_str(), name.size()) == 0)
        {
            std::cout << "Opening input port #" << i << ": '" << portName << "'\n";
            rtmidi->openPort(i);
            return true;
        }
    }
    return false;
}

static bool chooseMidiOutPort (RtMidiOut *rtmidi, std::string name)
{
    std::string portName;
    unsigned int i = 0, nPorts = rtmidi->getPortCount ();
    for ( i=0; i<nPorts; i++ ) {
        portName = rtmidi->getPortName(i);
        std::cout << "chooseMidiOutPort: found '" << portName << "'\n";
        if (strncmp (portName.c_str(), name.c_str(), name.size()) == 0)
        {
            std::cout << "Opening output port #" << i << ": '" << portName << "'\n";
            rtmidi->openPort(i);
            return true;
        }
    }
    return false;
}

static int midi_connect_in (MidiDevice *dev)
{
    try {
        if (chooseMidiInPort(dev->midiin, dev->name) == false)
            return 0;

        dev->midiin->setCallback (& mycallback, (void *) dev);
        dev->midiin->ignoreTypes (false, false, false);

    } catch (RtMidiError &error) {
        error.printMessage();
        return 0;
    }
    return 1;
}

static int midi_connect_out (MidiDevice *dev)
{
    try {
        if (chooseMidiOutPort(dev->midiout, dev->name) == false)
            return 0;
    }
    catch (RtMidiError &error) {
        error.printMessage();
        return 0;
    }
    return 1;
}

void *midi_init (const char *name)
{
    MidiDevice *dev = new MidiDevice;
    dev->name = name;
    std::cout << "dev->name: '" << dev->name << "'\n";
    dev->inConnected  = false;
    dev->outConnected = false;
    // RtMidiOut constructor
    try {
        dev->midiin  = new RtMidiIn();
        dev->midiout = new RtMidiOut();
    }
    catch ( RtMidiError &error ) {
        error.printMessage();
        exit( EXIT_FAILURE );
    }

    return dev;
}

void midi_connect (void *_dev)
{
    MidiDevice *dev = (MidiDevice *) _dev;
    if (!dev->inConnected)
        dev->inConnected = midi_connect_in (dev);

    if (!dev->outConnected)
        dev->outConnected = midi_connect_out (dev);
}


int midi_get_message (void *_dev, uint8_t *msg, int *len)
{
    MidiDevice *dev = (MidiDevice *) _dev;
    if (dev->queue.empty ())
        return 0;
    std::vector<unsigned char> message = dev->queue.front ();
    dev->queue.pop ();

    *len = message.size();
    for (int i=0; i<*len; i++)
        msg[i] = message.at(i);
    return 1;
}


void midi_cleanup (void *_dev)
{
    MidiDevice *dev = (MidiDevice *) _dev;
    if (dev)
    {
        if (dev->midiin)
            delete dev->midiin;
        if (dev->midiout)
            delete dev->midiout;
    }
    delete dev;
}

int midi_send_message (void *_dev, uint8_t *buf, int len)
{
    MidiDevice *dev = (MidiDevice *) _dev;
    if (!dev->outConnected)
        dev->outConnected = midi_connect_out (dev);
    if (!dev->outConnected)
        return -1;

    std::vector<unsigned char> message;
    for (int i=0; i<len; i++)
        message.push_back( buf[i] );
    dev->midiout->sendMessage( &message );
    return 0;
}
