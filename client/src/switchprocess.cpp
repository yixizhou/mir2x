/*
 * =====================================================================================
 *
 *       Filename: switchprocess.cpp
 *        Created: 01/23/2016 04:18:45
 *  Last Modified: 03/20/2016 01:45:20
 *
 *    Description: 
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#include "game.hpp"
#include "processlogo.hpp"
#include "processsyrc.hpp"

void Game::SwitchProcess(int nNewID)
{
    if(m_CurrentProcess){
        SwitchProcess(m_CurrentProcess->ID(), nNewID);
    }else{
        SwitchProcess(PROCESSID_NULL, nNewID);
    }
}

void Game::SwitchProcess(int nOldID, int nNewID)
{
    delete m_CurrentProcess;
    m_CurrentProcess = nullptr;

    switch(nOldID)
    {
        case PROCESSID_NULL:
            {
                switch(nNewID)
                {
                    case PROCESSID_LOGO:
                        {
                            // on initialization
                            m_CurrentProcess = new ProcessLogo();
                            SDL_ShowCursor(0);
                            break;
                        }
                    case PROCESSID_EXIT:
                        {
                            // exit immediately when logo shows
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }

        case PROCESSID_LOGO:
            {
                switch(nNewID)
                {
                    case PROCESSID_SYRC:
                        {
                            // on initialization
                            m_CurrentProcess = new ProcessSyrc();
                            SDL_ShowCursor(1);
                            break;
                        }
                    case PROCESSID_EXIT:
                        {
                            // exit immediately when logo shows
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }

        case PROCESSID_SYRC:
            {
                switch(nNewID)
                {
                    case PROCESSID_LOGIN:
                        {
                            SDL_ShowCursor(1);
                            break;
                        }
                    default:
                        {
                            break;
                        }

                }
                break;
            }
        default:
            {
                break;
            }
    }
}
