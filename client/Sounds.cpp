#include <stdio.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h>

#include "Sounds.h"

//initialize static class variables
void* Sound::device = nullptr;
void* Sound::context = nullptr;
std::vector<unsigned int> Sound::buffers;
std::vector<unsigned int> Sound::sources;

void Sound::init(int argc, char **argv){
  alutInit(&argc,argv);
  device = (void*) alcOpenDevice(NULL);
  if(!device) printf("Error: couldn't find an audio device\n");
  context = (void*)alcCreateContext((ALCdevice*)device,NULL);
  if(!alcMakeContextCurrent((ALCcontext*)context)) printf("Error: couldn't take control of audio\n");
}

void Sound::cleanup(){
  for(unsigned int i=0;i<buffers.size();i++){
    alDeleteBuffers(1, (ALuint*)&buffers[i]);
    alDeleteSources(1, (ALuint*)&sources[i]);
  }

  alcMakeContextCurrent(NULL);
  alcDestroyContext((ALCcontext*)context);
  alcCloseDevice((ALCdevice*)device);
  alutExit();
}

Sound::Sound(const char* filename, bool loop){
  alGenSources((ALuint)1,(ALuint*)&source);
  alSourcef(source,AL_PITCH,1);
  alSourcef(source,AL_GAIN,1);
  alSource3f(source,AL_POSITION,0,0,0);
  alSource3f(source,AL_VELOCITY,0,0,0);
  alSourcei(source,AL_LOOPING,AL_FALSE);

  buffer = alutCreateBufferFromFile(filename);

  buffers.push_back(buffer);
  sources.push_back(source);

  looping = loop;
}

void Sound::play(bool blocking){
  alSourcei((ALuint)source,AL_BUFFER,(ALuint)buffer);
  if(looping && !blocking) alSourcei((ALuint)source,AL_LOOPING,1);
  else alSourcei((ALuint)source,AL_LOOPING,0);

  alSourcePlay((ALuint)source);
  if(blocking){
    int source_state;
    alGetSourcei((ALuint)source, AL_SOURCE_STATE, &source_state);
    while (source_state == AL_PLAYING) {
      alGetSourcei((ALuint)source, AL_SOURCE_STATE, &source_state);
    }
  }
}

bool Sound::checkError(){
  return (alGetError() != AL_NO_ERROR || alutGetError() != ALUT_ERROR_NO_ERROR);
}
