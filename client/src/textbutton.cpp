/*
 * =====================================================================================
 *
 *       Filename: textbutton.cpp
 *        Created: 03/16/2017 15:04:17
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

#include "log.hpp"
#include "strfunc.hpp"
#include "mathfunc.hpp"
#include "sdldevice.hpp"
#include "textbutton.hpp"
#include "tritexbutton.hpp"

extern Log *g_Log;
extern SDLDevice *g_SDLDevice;

void TextButton::DrawEx(int nDstX, int nDstY, int nSrcX, int nSrcY, int nW, int nH)
{
    extern SDLDevice *g_SDLDevice;
    g_SDLDevice->PushColor(m_Color[State()][1]);
    g_SDLDevice->FillRectangle(nDstX, nDstY, nW, nH);
    g_SDLDevice->PopColor();

    g_SDLDevice->PushColor(m_FrameLineColor[State()]);
    g_SDLDevice->DrawRectangle(m_FrameLineWidth, nDstX, nDstY, nW, nH);
    g_SDLDevice->PopColor();

    int nLBX0 = (W() - m_Label.W()) / 2;
    int nLBY0 = (H() - m_Label.H()) / 2;

    int nLBW0 = m_Label.W();
    int nLBH0 = m_Label.H();

    int nLBX = nLBX0;
    int nLBY = nLBY0;

    int nLBW = nLBW0;
    int nLBH = nLBH0;

    if(RectangleOverlapRegion(nSrcX, nSrcY, nW, nH, &nLBX, &nLBY, &nLBW, &nLBH)){
        m_Label.SetColor(m_Color[State()][0]);
        m_Label.DrawEx(nDstX + (nLBX - nSrcX) + OffX(), nDstY + (nLBY - nSrcY) + OffY(), nLBX - nLBX0, nLBY - nLBY0, nLBW, nLBH);
    }
}

void TextButton::FormatText(const char *szFormatText, ...)
{
    std::string szText;
    std::string szError;
    {
        va_list ap;
        va_start(ap, szFormatText);

        try{
            szText = str_vprintf(szFormatText, ap);
        }catch(const std::exception &e){
            szText = "INTERNAL_ERROR";
            szError = str_printf("Exception caught in TextButton::FormatText(\"%s\", ...): %s", szFormatText, e.what());
        }

        va_end(ap);
    }

    if(!szError.empty()){
        g_Log->AddLog(LOGTYPE_WARNING, "%s", szError.c_str());
    }

    m_Label.FormatText("%s", szText.c_str());
    m_W = std::max<int>(m_W, m_Label.W());
    m_H = std::max<int>(m_H, m_Label.H());
}
