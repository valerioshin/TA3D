/*  TA3D, a remake of Total Annihilation
	Copyright (C) 2006  Roland BROCHARD

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA*/

#include <stdafx.h>
#include <ta3dbase.h>			// just to use the global camera object
#include <ingame/sidedata.h>
#include <misc/math.h>
#include "manager.h"
#include <logs/logs.h>
#include <misc/camera.h>
#include <misc/paths.h>
#include <QFile>


using namespace TA3D::Interfaces;



TA3D::Audio::Manager::Ptr TA3D::VARS::sound_manager;


namespace TA3D
{
namespace Audio
{

	const int nbChannels = 16;

	Manager::Manager()
		:m_SDLMixerRunning( false ), m_InBattle(false), pBattleTunesCount(0),
        pMusic( NULL ), bPlayMusic(false), pBasicSound( NULL ),
        pCurrentItemToPlay(-1), pCurrentItemPlaying(-1)
	{
		pMinTicks = 500;

		doStartUpAudio();
		if (isRunning())
			InitInterface();
	}




	void Manager::setPlayListFileMode(const int idx, bool inBattle, bool disabled)
	{
		if (idx < 0 || idx >= (int)pPlaylist.size())
			return;

		inBattle &= !disabled;
		if (pPlaylist[idx]->battleTune && !inBattle)
			--pBattleTunesCount;
		else
		{
			if (!pPlaylist[idx]->battleTune && inBattle)
				++pBattleTunesCount;
		}

		pPlaylist[idx]->battleTune = inBattle;
		pPlaylist[idx]->disabled = disabled;
	}



    bool Manager::getPlayListFiles(QStringList& out)
	{
        out.clear();
        out.reserve(pPlaylist.size());
        int indx(-1);
        for (PlaylistItem *i : pPlaylist)
		{
            ++indx;
            QString name = i->filename;
			if (pPlaylist[indx]->cdromID >= 0)
                name = "[CD] " + name;
			if (pPlaylist[indx]->battleTune)
                out.push_back("[B] " + name);
			else
			{
				if (pPlaylist[indx]->disabled)
                    out.push_back("[ ] " + name);
				else
                    out.push_back("[*] " + name);
			}
		}
        return !out.isEmpty();
	}



	void Manager::updatePlayListFiles()
	{
		pMutex.lock();
		doUpdatePlayListFiles();
		pMutex.unlock();
	}

	void Manager::doUpdatePlayListFiles()
	{
		MutexLocker locker(pMutex);

        QStringList file_list;
		VFS::Instance()->getFilelist("music/*.ogg", file_list);
		VFS::Instance()->getFilelist("music/*.mp3", file_list);
		VFS::Instance()->getFilelist("music/*.mid", file_list);
		VFS::Instance()->getFilelist("music/*.wav", file_list);
		VFS::Instance()->getFilelist("music/*.mod", file_list);

        std::sort(file_list.begin(), file_list.end());

		for (Playlist::iterator i = pPlaylist.begin(); i != pPlaylist.end(); ++i)
			(*i)->checked = false;
		bool default_deactivation = !pPlaylist.empty();

		QString filename;
        for (QStringList::iterator i = file_list.begin() ; i != file_list.end() ; ++i) // Add missing files
		{
			*i = Paths::ExtractFileName(*i);
			if (ToLower(*i) == "playlist.txt" || (*i)[0] == '.')
				continue;

			filename = *i;

			Playlist::const_iterator j;
			for (j = pPlaylist.begin(); j != pPlaylist.end(); ++j)
			{
				if ((*j)->filename == filename)
				{
					(*j)->checked = true;
					break;
				}
			}

			if (j == pPlaylist.end()) // It's missing, add it
			{
				PlaylistItem *m_Tune = new PlaylistItem();
				m_Tune->battleTune = false;
				m_Tune->disabled = default_deactivation;
				m_Tune->checked = true;
				m_Tune->filename = filename;
				logs.debug() << LOG_PREFIX_SOUND << "Added to the playlist: `" << filename << '`';
				pPlaylist.push_back(m_Tune);
			}
		}

		// Check for audio tracks in cdrom drives
		int nbDrives = SDL_CDNumDrives();
		for(int i = 0 ; i < nbDrives ; ++i)
		{
			SDL_CD *cd = SDL_CDOpen(i);
			if (!CD_INDRIVE(SDL_CDStatus(cd)))
			{
				SDL_CDClose(cd);
				continue;
			}
			LOG_INFO(LOG_PREFIX_SOUND << "cdrom found : " << SDL_CDName(i));
			LOG_INFO(LOG_PREFIX_SOUND << cd->numtracks << " tracks found");
			for(int e = 0 ; e < cd->numtracks ; ++e)
			{
				LOG_INFO(LOG_PREFIX_SOUND << "track " << e << " is " << (cd->track[e].type == SDL_AUDIO_TRACK ? "audio" : "data"));
				if (cd->track[e].type != SDL_AUDIO_TRACK)   continue;

                const QString &name = QString(SDL_CDName(i)) + QString("_%1").arg(e);
                Playlist::const_iterator it;
                for (it = pPlaylist.begin(); it != pPlaylist.end(); ++it)
				{
                    if ((*it)->filename == name)
					{
                        (*it)->checked = true;
						break;
					}
				}

				PlaylistItem *m_Tune = (it == pPlaylist.end()) ? new PlaylistItem() : *it;      // We have to update things
				if (it == pPlaylist.end())
				{
					m_Tune->battleTune = false;
					m_Tune->disabled = default_deactivation;
					m_Tune->checked = true;
				}
				m_Tune->filename = name;
				m_Tune->cdromID = i;
				m_Tune->trackID = e;
				if (it == pPlaylist.end()) // It's missing, add it
					pPlaylist.push_back(m_Tune);
			}

			SDL_CDClose(cd);
		}

		int e = 0;
		for (unsigned int i = 0 ; i + e < pPlaylist.size() ; ) // Do some cleaning
		{
			if (pPlaylist[i + e]->checked)
			{
				pPlaylist[i] = pPlaylist[i + e];
				++i;
			}
			else
			{
				delete pPlaylist[i + e];
				++e;
			}
		}

		pPlaylist.resize(pPlaylist.size() - e);	// Remove missing files
		doSavePlaylist();
	}


	void Manager::savePlaylist()
	{
		pMutex.lock();
		doSavePlaylist();
		pMutex.unlock();
	}

	void Manager::doSavePlaylist()
	{
        QString targetPlaylist = TA3D::Paths::Resources + "music/playlist.txt";
		// Make sure the folder exists
        Paths::MakeDir(Paths::ExtractFilePath(targetPlaylist));
        QFile play_list_file(targetPlaylist);
        play_list_file.open(QIODevice::Truncate | QIODevice::WriteOnly);
        if (!play_list_file.isOpen())
		{
			LOG_ERROR(LOG_PREFIX_SOUND << "could not open playlist file : '" << targetPlaylist << "'");
			return;
		}

        QTextStream stream(&play_list_file);
        stream << "#this file has been generated by TA3D_Audio module\n";
		for (Playlist::const_iterator i = pPlaylist.begin(); i != pPlaylist.end(); ++i)
		{
			if ((*i)->battleTune)
                stream << "*";
			else
			{
				if ((*i)->disabled)
                    stream << "!";
			}
            stream << (*i)->filename << "\n";
        }
        stream.flush();
		play_list_file.flush();
		play_list_file.close();

		LOG_INFO(LOG_PREFIX_SOUND << "playlist saved");
	}




	void Manager::doLoadPlaylist()
	{
        QString filename = TA3D::Paths::Resources + "music/playlist.txt";
        QFile file(filename);
        file.open(QIODevice::ReadOnly);

        if (!file.isOpen()) // try to create the list if it doesn't exist
		{
			doUpdatePlayListFiles();
            file.open(QIODevice::ReadOnly);
            if (!file.isOpen())
			{
				LOG_WARNING(LOG_PREFIX_SOUND << "Impossible to load the playlist : '" << filename << "'");
				return;
			}
		}

		LOG_INFO(LOG_PREFIX_SOUND << "Loading the playlist...");

		bool isBattle(false);
		bool isActivated(true);

		pBattleTunesCount = 0;

        while (!file.atEnd())
		{
            QByteArray line = file.readLine();

            line = line.trimmed(); // strip off spaces, linefeeds, tabs, newlines

            if (line.isEmpty())
				continue;
			if (line[0] == '#' || line[0] == ';')
				continue;

			isActivated = true;

			if (line[0] == '*')
			{
				isBattle = true;
                line.remove(0, 1);
				++pBattleTunesCount;
			}
			else
			{
				if (line[0] == '!')
				{
					isActivated = false;
                    line.remove(0, 1);
				}
				else
					isBattle = false;
			}

			PlaylistItem* m_Tune = new PlaylistItem();
			m_Tune->battleTune = isBattle;
			m_Tune->disabled = !isActivated;
            m_Tune->filename = QString::fromUtf8(line);

			logs.debug() << LOG_PREFIX_SOUND << "Added to the playlist: `" << line << '`';
			pPlaylist.push_back(m_Tune);
		}

		file.close();
		doUpdatePlayListFiles();
	}





	void Manager::doShutdownAudio(const bool purgeLoadedData)
	{
		if (m_SDLMixerRunning) // only execute stop if we are running.
			doStopMusic();

		if (purgeLoadedData)
		{
			purgeSounds(); // purge sound list.
			doPurgePlaylist(); // purge play list
		}

		if (m_SDLMixerRunning)
		{
			Mix_AllocateChannels(0);

			Mix_CloseAudio();
			DeleteInterface();
			m_SDLMixerRunning = false;
			pMusic = NULL;
		}

        SDL_QuitSubSystem( SDL_INIT_CDROM );
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
	}




	bool Manager::doStartUpAudio()
	{
		if (lp_CONFIG->no_sound)
			return false;

		pMusic = NULL;
		fCounter = 0;
        bPlayMusic = false;

		if (m_SDLMixerRunning)
			return true;

		if (!SDL_WasInit(SDL_INIT_AUDIO))
		{
			if (SDL_InitSubSystem( SDL_INIT_AUDIO ))
			{
				logs.error() << LOG_PREFIX_SOUND << "SDL_InitSubSystem( SDL_INIT_AUDIO ) failed: " << SDL_GetError();
				return false;
			}
		}
        if (!SDL_WasInit(SDL_INIT_CDROM))
        {
            if (SDL_InitSubSystem( SDL_INIT_CDROM ))
            {
                logs.error() << LOG_PREFIX_SOUND << "SDL_InitSubSystem( SDL_INIT_CDROM ) failed: " << SDL_GetError();
                return false;
            }
        }
        if (!SDL_WasInit(SDL_INIT_TIMER))
        {
            if (SDL_InitSubSystem( SDL_INIT_TIMER ))
            {
                logs.error() << LOG_PREFIX_SOUND << "SDL_InitSubSystem( SDL_INIT_TIMER ) failed: " << SDL_GetError();
                return false;
            }
        }

		// 44.1KHz, signed 16bit, system byte order,
		// stereo, 4096 bytes for chunks
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1)
        {
			logs.error() << LOG_PREFIX_SOUND << "Mix_OpenAudio: " << Mix_GetError();
			return false;
		}

        pCurrentItemPlaying = -1;

		Mix_AllocateChannels(nbChannels);

		SDL_version compiled_version;
		const SDL_version *linked_version;
		MIX_VERSION(&compiled_version);
		logs.debug() << LOG_PREFIX_SOUND << "compiled with SDL_mixer version: " << (int)compiled_version.major << '.' << (int)compiled_version.minor << '.' << (int)compiled_version.patch;
		linked_version = Mix_Linked_Version();
		logs.debug() << LOG_PREFIX_SOUND << "running with SDL_mixer version: " << (int)linked_version->major << '.' << (int)linked_version->minor << '.' << (int)linked_version->patch;

		m_SDLMixerRunning = true;
		doLoadPlaylist();

		setVolume(lp_CONFIG->sound_volume);
		setMusicVolume(lp_CONFIG->music_volume);
		return true;
	}



	Manager::~Manager()
	{
		doShutdownAudio(true);
	}



	void Manager::stopMusic()
	{
		pMutex.lock();
        bPlayMusic = false;
        doStopMusic();
		pMutex.unlock();
	}

	void Manager::doStopMusic()
	{
		if (m_SDLMixerRunning && pMusic != NULL)
		{
			Mix_HaltMusic();
			Mix_FreeMusic(pMusic);
			pMusic = NULL;
		}
        else if (m_SDLMixerRunning && pCurrentItemPlaying >= 0
                 && pPlaylist[pCurrentItemPlaying]->cdromID >= 0
                 && pPlaylist[pCurrentItemPlaying]->cd)        // We're playing a track from an audio CD
        {
            SDL_CDStop(pPlaylist[pCurrentItemPlaying]->cd);
            SDL_CDClose(pPlaylist[pCurrentItemPlaying]->cd);
            pPlaylist[pCurrentItemPlaying]->cd = NULL;
        }
	}




	void Manager::doPurgePlaylist()
	{
		pMutex.lock();
		doStopMusic();
		pCurrentItemToPlay = -1;
		// we don't change this in stop music in case
		// we want to do a play and contine through our list, so
		// we change it here to refelect no index.

		if (!pPlaylist.empty())
		{
			for (Playlist::iterator k_Pos = pPlaylist.begin(); k_Pos != pPlaylist.end(); ++k_Pos)
				delete *k_Pos;
			pPlaylist.clear();
		}
		pMutex.unlock();
	}




	void Manager::togglePauseMusic()
	{
		pMutex.lock();
		if (m_SDLMixerRunning && pMusic != NULL)
		{
			if (Mix_PausedMusic())
				Mix_PauseMusic();
			else
				Mix_ResumeMusic();
		}
		else if (m_SDLMixerRunning && pCurrentItemPlaying >= 0
				 && pPlaylist[pCurrentItemPlaying]->cdromID >= 0
				 && pPlaylist[pCurrentItemPlaying]->cd)
		{
			if (SDL_CDStatus(pPlaylist[pCurrentItemPlaying]->cd) == CD_PLAYING)
				SDL_CDPause(pPlaylist[pCurrentItemPlaying]->cd);
			else if (SDL_CDStatus(pPlaylist[pCurrentItemPlaying]->cd) == CD_PAUSED)
				SDL_CDResume(pPlaylist[pCurrentItemPlaying]->cd);
		}
		pMutex.unlock();
	}



	void Manager::pauseMusic()
	{
		pMutex.lock();
		doPauseMusic();
		pMutex.unlock();
	}

	void Manager::doPauseMusic()
	{
		if (m_SDLMixerRunning && pMusic != NULL)
			Mix_PauseMusic();
		else if (m_SDLMixerRunning && pCurrentItemPlaying >= 0
				 && pPlaylist[pCurrentItemPlaying]->cdromID >= 0
				 && pPlaylist[pCurrentItemPlaying]->cd)
			SDL_CDPause(pPlaylist[pCurrentItemPlaying]->cd);
	}




	QString Manager::doSelectNextMusic()
	{
		if (pPlaylist.empty())
			return nullptr;

		sint16 cIndex = -1;
		sint16 mCount = 0;
		QString szResult;
		if (m_InBattle && pBattleTunesCount > 0)
		{
			srand((unsigned)time(NULL));
			cIndex =  (sint16)((TA3D_RAND() % pBattleTunesCount) + 1);
			mCount = 1;

			for (Playlist::const_iterator cur = pPlaylist.begin(); cur != pPlaylist.end(); ++cur)
			{
				if ((*cur)->battleTune && mCount >= cIndex)		// If we get one that match our needs we take it
				{
					if ((*cur)->cdromID >= 0)
						szResult = (*cur)->filename;
					else
                        szResult = VFS::Instance()->extractFile("music/" + (*cur)->filename);
					break;
				}
				else
				{
					if ((*cur)->battleTune) // Take the last one that can be taken if we try to go too far
                        szResult = (*cur)->cdromID >= 0 ? (*cur)->filename : VFS::Instance()->extractFile("music/" + (*cur)->filename);
				}
			}
			return szResult;
		}

		mCount = 0;
		if (pCurrentItemToPlay > (sint32)pPlaylist.size())
			pCurrentItemToPlay = -1;

		bool found = false;

		for (Playlist::const_iterator cur = pPlaylist.begin(); cur != pPlaylist.end(); ++cur)
		{
			++mCount;
			if ((*cur)->battleTune || (*cur)->disabled)
				continue;

			if (pCurrentItemToPlay <= mCount || pCurrentItemToPlay <= 0)
			{
                szResult = (*cur)->cdromID >= 0 ? (*cur)->filename : VFS::Instance()->extractFile("music/" + (*cur)->filename);
				pCurrentItemToPlay = mCount + 1;
				found = true;
				break;
			}
		}
		if (!found && pCurrentItemToPlay != -1)
		{
			pCurrentItemToPlay = -1;
			return doSelectNextMusic();
		}
		return szResult;
	}




	void Manager::setMusicMode(const bool battleMode)
	{
		pMutex.lock();
		if (m_InBattle != battleMode)
		{
			m_InBattle = battleMode;
			doPlayMusic();
		}
		pMutex.unlock();
	}




	void Manager::doPlayMusic(const QString& filename)
	{
		doStopMusic();

        if (!m_SDLMixerRunning || filename.isEmpty())
			return;

		pCurrentItemPlaying = -1;
		for (uint32 i = 0; i < pPlaylist.size(); ++i)
		{
			if (pPlaylist[i]->filename == filename)
			{
				pCurrentItemPlaying = i;
				break;
			}
		}

		if (pCurrentItemPlaying >= 0 && pPlaylist[pCurrentItemPlaying]->cdromID >= 0)
		{
			SDL_CDClose(NULL);
			SDL_ClearError();
			pPlaylist[pCurrentItemPlaying]->cd = SDL_CDOpen(pPlaylist[pCurrentItemPlaying]->cdromID);
			if (pPlaylist[pCurrentItemPlaying]->cd == NULL)
				logs.error() << LOG_PREFIX_SOUND << "could not open cdrom " << pPlaylist[pCurrentItemPlaying]->cdromID <<  " : " << SDL_GetError();
			else
			{
				SDL_CDStatus(pPlaylist[pCurrentItemPlaying]->cd);
				if (SDL_CDPlayTracks(pPlaylist[pCurrentItemPlaying]->cd, pPlaylist[pCurrentItemPlaying]->trackID, 0, 1, 0))
					logs.debug() << LOG_PREFIX_SOUND << "Error playing track "  << pPlaylist[pCurrentItemPlaying]->trackID << " from CD " << SDL_CDName(pPlaylist[pCurrentItemPlaying]->cdromID) << " : " << SDL_GetError();
				else
					logs.debug() << LOG_PREFIX_SOUND << "Playing audio cd " << SDL_CDName(pPlaylist[pCurrentItemPlaying]->cdromID) <<  " track " << pPlaylist[pCurrentItemPlaying]->trackID;
				setMusicVolume( lp_CONFIG->music_volume );
			}
			return;
		}

		if (!Paths::Exists(filename))
		{
            if (!filename.isEmpty())
				logs.error() << LOG_PREFIX_SOUND << "Failed to find file: `" << filename << '`';
			return;
		}

        pMusic = Mix_LoadMUS( filename.toStdString().c_str() );

		if (pMusic == NULL)
		{
			logs.error() << LOG_PREFIX_SOUND << "Failed to open music file : `" << filename << "` (" << Mix_GetError() << ')';
			return;
		}

		if (Mix_PlayMusic(pMusic, 0) == -1)
		{
			logs.error() << LOG_PREFIX_SOUND << "Failed to play music file : `" << filename << "` (" << Mix_GetError() << ')';
			return;
		}

		logs.debug() << LOG_PREFIX_SOUND << "Playing music file " << filename;
		setMusicVolume( lp_CONFIG->music_volume );
	}



	void Manager::playMusic()
	{
		pMutex.lock();
        bPlayMusic = true;
        doPlayMusic();
		pMutex.unlock();
	}

	void Manager::doPlayMusic()
	{
        if (!m_SDLMixerRunning || !bPlayMusic)
			return;

        bool playing = false;

		if (pMusic != NULL)
		{
			if (Mix_PausedMusic())
			{
				Mix_ResumeMusic();
				return;
			}
            playing = Mix_PlayingMusic();
		}
		else
		{
			if (pCurrentItemPlaying >= 0
				&& pPlaylist[pCurrentItemPlaying]->cdromID >= 0 && pPlaylist[pCurrentItemPlaying]->cd)
			{
				if (SDL_CDStatus(pPlaylist[pCurrentItemPlaying]->cd) == CD_PAUSED)
				{
					SDL_CDResume(pPlaylist[pCurrentItemPlaying]->cd);
					return;
				}
				playing = SDL_CDStatus(pPlaylist[pCurrentItemPlaying]->cd) == CD_PLAYING;
			}
		}
		if (!playing)
			doPlayMusic(doSelectNextMusic());
	}



	// Begin sound managing routines.
	void Manager::setListenerPos(const Vector3D&)
	{
		// disabled because not used pMutex.unlock();
		// if (m_SDLMixerRunning)
		{
#warning TODO: implement 3D stereo
			//            FMOD_VECTOR pos     = { vec.x, vec.y, vec.z };
			//            FMOD_VECTOR vel     = { 0, 0, 0 };
			//            FMOD_VECTOR forward = { 0.0f, 0.0f, 1.0f };
			//            FMOD_VECTOR up      = { 0.0f, 1.0f, 0.0f };
			//
			//            pFMODSystem->set3DListenerAttributes(0, &pos, &vel, &forward, &up);
		}
	}

	void Manager::update3DSound()
	{
		MutexLocker locker(pMutex);
		doUpdate3DSound();
	}

	void Manager::doUpdate3DSound()
	{
		if (!m_SDLMixerRunning)
		{
			pWorkList.clear();
			return;
		}

#warning TODO: implement 3D stereo

		//        pFMODSystem->update();

		for (std::list< WorkListItem >::iterator i = pWorkList.begin(); i != pWorkList.end(); ++i)
		{
			if (Mix_PlayChannel(-1, i->sound->sampleHandle, 0) == -1)
				continue;

			if (i->sound->is3DSound)
			{
				//                FMOD_VECTOR pos = { i->vec->x, i->vec->y, i->vec->z };
				//                FMOD_VECTOR vel = { 0, 0, 0 };
				//                ch->set3DAttributes(&pos, &vel);
			}
		}

		pWorkList.clear();
		if ((fCounter++) < 100)
			return;

		fCounter = 0;

        if (((pMusic == NULL || !Mix_PlayingMusic()) && (pCurrentItemPlaying == -1 || pPlaylist[pCurrentItemPlaying]->cdromID < 0))
            || (pCurrentItemPlaying >= 0
                && pPlaylist[pCurrentItemPlaying]->cdromID >= 0
                && (pPlaylist[pCurrentItemPlaying]->cd == NULL
                    || SDL_CDStatus(pPlaylist[pCurrentItemPlaying]->cd) != CD_PLAYING)) )
		{
			doPlayMusic();
			return;
		}
	}


	uint32 Manager::InterfaceMsg(const uint32 MsgID, const QString &msg)
	{
		if (MsgID == TA3D_IM_GUI_MSG)	// for GUI messages, test if it's a message for us
		{
            if (msg.isEmpty())
				return INTERFACE_RESULT_HANDLED; // Oups badly written things

			// Get the string associated with the signal
            const QString &message = msg.toLower();

			if (message == "music play")
			{
				doPlayMusic();
				return INTERFACE_RESULT_HANDLED;
			}
			if (message == "music pause")
			{
				doPauseMusic();
				return INTERFACE_RESULT_HANDLED;
			}
			if (message == "music stop")
			{
				doStopMusic();
				return INTERFACE_RESULT_HANDLED;
			}
		}
		return INTERFACE_RESULT_CONTINUE;
	}




	void Manager::playSoundFileNow(const QString& filename)
	{
		stopSoundFileNow();

        QIODevice *file = VFS::Instance()->readFile(filename);
		if (file)
		{
            const QByteArray &buffer = file->readAll();
            pBasicSound = Mix_LoadWAV_RW( SDL_RWFromMem((void*)buffer.data(), buffer.size()), 1);
			delete file;
			if (pBasicSound == NULL)
			{
				logs.error() << LOG_PREFIX_SOUND << "error loading file `" << filename << "` (" << Mix_GetError() << ')';
				return;
			}
			Mix_PlayChannel(-1, pBasicSound, 0);
		}
	}


	void Manager::stopSoundFileNow()
	{
		MutexLocker locker(pMutex);
		if (pBasicSound)
		{
			for (int i = 0; i < nbChannels; ++i)
			{
				if (Mix_GetChunk(i) == pBasicSound)
					Mix_HaltChannel(i);
			}
			Mix_FreeChunk(pBasicSound);
		}
		pBasicSound = NULL;
	}


	bool Manager::loadSound(const QString& filename, const bool LoadAs3D, const float MinDistance, const float MaxDistance)
	{
		MutexLocker locker(pMutex);
		return doLoadSound(filename, LoadAs3D, MinDistance, MaxDistance);
	}

	bool Manager::doLoadSound(QString filename, const bool LoadAs3D, const float /*MinDistance*/, const float /*MaxDistance*/)
	{
        if (filename.isEmpty())       // We can't load a file with an empty name
			return false;
        filename = filename.toLower();

		// if it has a .wav extension then remove it.
		if (filename.endsWith(".wav") || filename.endsWith(".ogg") || filename.endsWith(".mp3"))
			filename = Substr(filename, 0, filename.length() - 4);

		// if its already loaded return true.
		if (pSoundList.count(filename) != 0)
		{
			return true;
		}

		// pull the data from hpi.
        QString theSound = "sounds/" + filename;
        if (VFS::Instance()->fileExists(theSound + ".wav"))
            theSound += ".wav";
        else if (VFS::Instance()->fileExists(theSound + ".ogg"))
            theSound += ".ogg";
        else if (VFS::Instance()->fileExists(theSound + ".mp3"))
            theSound += ".mp3";
        QIODevice* file = VFS::Instance()->readFile(theSound);
		if (!file) // if no data, log a message and return false.
		{
			// logs.debug() <<  LOG_PREFIX_SOUND << "Manager: LoadSound(" << filename << "), no such sound found in HPI.");
			return false;
		}

		SoundItemList* it = new SoundItemList(LoadAs3D);
		LOG_ASSERT(NULL != it);

		// Now get SDL_mixer to load the sample
        const QByteArray &buffer = file->readAll();
        it->sampleHandle = Mix_LoadWAV_RW( SDL_RWFromMem((void*)buffer.data(), buffer.size()), 1 );
		delete file; // we no longer need this.

		if (it->sampleHandle == NULL) // ahh crap SDL_mixer couln't load it.
		{
			delete it;  // delete the sound.
			// log a message and return false;
			if (m_SDLMixerRunning)
				logs.debug() << LOG_PREFIX_SOUND << "Manager: LoadSound(" << filename << "), Failed to construct sample.";
			return false;
		}

		// if its a 3d Sound we need to set min/max distance.
#warning TODO: implement 3D stereo
		//        if (it->is3DSound)
		//            it->sampleHandle->set3DMinMaxDistance(MinDistance, MaxDistance);

		// add the sound to our soundlist hash table, and return true.
		pSoundList[filename] = it;
		return true;
	}


	void Manager::loadTDFSounds(const bool allSounds)
	{
		if (lp_CONFIG->no_sound)
			return;

		MutexLocker locker(pMutex);
		// Which file to load ?
		QString filename(ta3dSideData.gamedata_dir);
		filename += (allSounds) ? "allsound.tdf" : "sound.tdf";

		logs.debug() << LOG_PREFIX_SOUND << "Reading `" << filename << "`...";
		// Load the TDF file
		if (pTable.loadFromFile(filename))
		{
			LOG_INFO(LOG_PREFIX_SOUND << "Loading sounds from " << filename);
			// Load each sound file
			pTable.forEach(LoadAllTDFSound(*this));
			logs.debug() << LOG_PREFIX_SOUND << "Reading: Done.";
		}
		else
			logs.debug() << LOG_PREFIX_SOUND << "Reading: Aborted.";
	}


	void Manager::purgeSounds()
	{
		pMutex.lock();

		Mix_HaltChannel(-1);

		for(TA3D::UTILS::HashMap<SoundItemList*>::Dense::iterator it = pSoundList.begin() ; it != pSoundList.end() ; ++it)
			delete *it;
		pSoundList.clear();
		pTable.clear();
		pWorkList.clear();
		pMutex.unlock();
	}



	// Play sound directly from our sound pool
	void Manager::playSound(const QString& filename, const Vector3D* vec)
	{
        if (filename.isEmpty())
			return;

		MutexLocker locker(pMutex);
		if (vec && Camera::inGame && ((Vector3D)(*vec - Camera::inGame->rpos)).sq() > 360000.0f) // If the source is too far, does not even think about playing it!
			return;
		if (!m_SDLMixerRunning)
			return;

		QString szWav(filename); // copy string to szWav so we can work with it.
        if (szWav.toLower().contains(".wav")           // if it has a .wav/.ogg/.mp3 extension then remove it.
                || szWav.toLower().contains(".ogg")
                || szWav.toLower().contains(".mp3"))
			szWav.truncate(szWav.length() - 4);

		SoundItemList* sound = pSoundList[szWav];
		if (!sound)
		{
			logs.error() << LOG_PREFIX_SOUND << "`" << filename << "` not found, aborting";
			return;
		}

		if (msec_timer - sound->lastTimePlayed < pMinTicks)
			return; // Make sure it doesn't play too often, so it doesn't play too loud!

		sound->lastTimePlayed = msec_timer;

		if (!sound->sampleHandle || (sound->is3DSound && !vec))
		{
			if (!sound->sampleHandle)
				logs.error() << LOG_PREFIX_SOUND << "`" << filename << "` not played the good way";
			else
				logs.error() << LOG_PREFIX_SOUND << "`" << filename << "` sound->sampleHandle is false";
			return;
		}

		pWorkList.push_back(WorkListItem(sound, vec));
	}



	void Manager::playTDFSoundNow(const QString& Key, const Vector3D* vec)
	{
		pMutex.lock();
        QString szWav = pTable.pullAsString(Key.toLower()); // copy string to szWav so we can work with it.
        if (szWav.toLower().contains(".wav")
                || szWav.toLower().contains(".ogg")
                || szWav.toLower().contains(".mp3"))
			szWav.truncate(szWav.length() - 4);

		SoundItemList* it = pSoundList[szWav];
		if (it)
		{
			it->lastTimePlayed = msec_timer - 1000 - pMinTicks; // Make sure it'll be played
			doPlayTDFSound(Key, vec);
		}
		doUpdate3DSound();
		pMutex.unlock();
	}


	void Manager::playTDFSound(const QString& key, const Vector3D* vec)
	{
		pMutex.lock();
		doPlayTDFSound(key, vec);
		pMutex.unlock();
	}


	void Manager::doPlayTDFSound(QString key, const Vector3D* vec)
	{
        if (!key.isEmpty())
		{
			if (!pTable.exists(key.toLower()))
			{
				// output a report to the console but only once
				logs.warning() << LOG_PREFIX_SOUND << "Can't find key `" << key << '`';
				pTable.insertOrUpdate(key, "");
				return;
			}
            const QString& wav = pTable.pullAsString(key);
            if (!wav.isEmpty())
				playSound(wav, vec);
		}
	}


	void Manager::doPlayTDFSound(const QString& keyA, const QString& keyB, const Vector3D* vec)
	{
        if (!keyA.isEmpty() && !keyB.isEmpty())
		{
            const QString &key = keyA + '.' + keyB;
			doPlayTDFSound(key, vec);
		}
	}

	void Manager::playTDFSound(const QString& keyA, const QString& keyB, const Vector3D* vec)
	{
        if (!keyA.isEmpty() && !keyB.isEmpty())
		{
            const QString &key = keyA + '.' + keyB;
			playTDFSound(key, vec);
		}
	}


	Manager::SoundItemList::~SoundItemList()
	{
		if (sampleHandle)
		{
			for(int i = 0 ; i < nbChannels ; i++)
				if (Mix_GetChunk(i) == sampleHandle)
					Mix_HaltChannel(i);
			Mix_FreeChunk(sampleHandle);
		}
		sampleHandle = NULL;
	}


	void Manager::setVolume(int volume)
	{
		if (!isRunning())
			return;
		Mix_Volume(-1, volume);
	}


	void Manager::setMusicVolume(int volume)
	{
		if (!isRunning())
			return;
		Mix_VolumeMusic( volume );
	}




} // namespace Interfaces
} // namespace TA3D
