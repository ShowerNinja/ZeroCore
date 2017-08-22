///////////////////////////////////////////////////////////////////////////////
///
/// Author: Andrea Ellinger
/// Copyright 2017, DigiPen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "Precompiled.h"

namespace Audio
{
  //------------------------------------------------------------------------- Audio System Interface

  //************************************************************************************************
  AudioSystemInterface::AudioSystemInterface(ExternalSystemInterface* extInterface)
  {
    // Create the internal audio system
    System = new AudioSystemInternal(extInterface);
  }

  //************************************************************************************************
  AudioSystemInterface::~AudioSystemInterface()
  {
    // Delete the internal audio system
    delete System;
  }

  //************************************************************************************************
  void AudioSystemInterface::Update()
  {
    // Lock to change shared variables
    System->LockObject.Lock();

    // If we need to reset Port Audio, change the bool before unlocking, then reset
    if (System->ResetPA)
    {
      System->ResetPA = false;
      System->LockObject.Unlock();

      System->ResetIO();
    }
    // Otherwise just unlock
    else
      System->LockObject.Unlock();

    // Run tasks from the mix thread
    System->HandleTasks();

    // Delete objects on the to-be-deleted list
    forRange(TagObject* object, System->TagsToDelete.All())
      delete object;
    System->TagsToDelete.Clear();
  }

  //************************************************************************************************
  void AudioSystemInterface::StartSystem(Zero::Status &status)
  {
    System->StartSystem(status);
  }

  //************************************************************************************************
  void AudioSystemInterface::StopSystem(Zero::Status &status)
  {
    System->StopSystem(status);
  }

  //************************************************************************************************
  float AudioSystemInterface::GetVolume()
  {
    // Return the non-threaded volume variable
    return System->Volume;
  }

  //************************************************************************************************
  void AudioSystemInterface::SetVolume(const float newVolume)
  {
    // Set the non-threaded volume variable
    System->Volume = newVolume;

    // Send an asynchronous task to the threaded system
    System->AddTask(Zero::CreateFunctor(&AudioSystemInternal::SystemVolumeThreaded, System, newVolume));
  }

  //************************************************************************************************
  unsigned AudioSystemInterface::GetSampleRate()
  {
    return AudioSystemInternal::SampleRate;
  }

  //************************************************************************************************
  unsigned AudioSystemInterface::GetOutputChannels()
  {
    return System->SystemChannelsThreaded;
  }

  //************************************************************************************************
  void AudioSystemInterface::SetOutputChannels(const unsigned channels)
  {
    // Can only mix channels up to 7.1
    if (channels <= 8)
    {
      if (channels == 0)
        System->AddTask(Zero::CreateFunctor(&AudioSystemInternal::SetSystemChannelsThreaded, System, 
          System->AudioIO.OutputChannelsThreaded));
      else
        System->AddTask(Zero::CreateFunctor(&AudioSystemInternal::SetSystemChannelsThreaded, System, 
          channels));
    }
  }

  //************************************************************************************************
  void AudioSystemInterface::AddNodeToOutput(SoundNode* node)
  {
    if (System->FinalOutputNode)
      System->FinalOutputNode->AddInput(node);
  }

  //************************************************************************************************
  void AudioSystemInterface::RemoveNodeFromOutput(SoundNode* node)
  {
    if (System->FinalOutputNode)
      System->FinalOutputNode->RemoveInput(node);
  }

  //************************************************************************************************
  float AudioSystemInterface::GetPeakOutputVolume()
  {
    return System->PeakVolumeLastMix;
  }

  //************************************************************************************************
  float AudioSystemInterface::GetRMSOutputVolume()
  {
    return System->RmsVolumeLastMix;
  }

  //************************************************************************************************
  void AudioSystemInterface::SetMinimumVolumeThreshold(const float volume)
  {
    System->AddTask(Zero::CreateFunctor(&AudioSystemInternal::SetMinVolumeThresholdThreaded, System, 
      volume));
  }

  //************************************************************************************************
  void AudioSystemInterface::UseHighLatency(const bool useHigh)
  {
    System->AddTask(Zero::CreateFunctor(&AudioSystemInternal::SetUseHighLatency, System, useHigh));
  }

  //------------------------------------------------------------------------------------- Audio File

  //************************************************************************************************
  AudioFile::AudioFile() :
    Channels(0),
    FileLength(0)
  {
    ZeroConstructPrivateData(AudioFileData);
  }

  //************************************************************************************************
  AudioFile::~AudioFile()
  {
    Close();
    ZeroDestructPrivateData(AudioFileData);
  }

  //************************************************************************************************
  void AudioFile::OpenFile(Zero::Status& status, Zero::StringParam fileName)
  {
    ZeroGetPrivateData(AudioFileData);

    if (self->BuffersPerChannel)
    {
      status.SetFailed("Already open, call Close before opening another file");
    }
    else
    {
      *self = FileEncoder::OpenFile(status, fileName);

      if (status.Succeeded())
      {
        Channels = self->Channels;
        FileLength = (float)self->SamplesPerChannel / (float)self->SampleRate;
      }
    }
  }

  //************************************************************************************************
  void AudioFile::WriteEncodedFile(Zero::Status& status, Zero::StringParam outputFileName, 
    bool normalize, float maxVolume)
  {
    ZeroGetPrivateData(AudioFileData);

    if (self->BuffersPerChannel)
      FileEncoder::WriteFile(status, outputFileName, *self, normalize, maxVolume);
    else
      status.SetFailed("No input file was opened");
  }

  //************************************************************************************************
  void AudioFile::Close()
  {
    ZeroGetPrivateData(AudioFileData);

    self->ReleaseData();
  }

}