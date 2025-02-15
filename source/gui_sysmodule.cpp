#include "gui_sysmodule.hpp"
#include "button.hpp"

#include <stdio.h>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include <utility>

#include "list_selector.hpp"
#include "message_box.hpp"

#include "json.hpp"

using json = nlohmann::json;

extern "C" {
  #include "pm_dmnt.h"
  #include "hid_extra.h"
}

static bool anyModulesPresent = false;
u16 pagexoffset = 100;
u16 pagextooffset = 500;
int currentpage = 1;
int pagescount = 3;

GuiSysmodule::GuiSysmodule() : Gui() {
  pmshellInitialize();
  pmdmntInitialize();

  anyModulesPresent = false;

  std::ifstream configFile("sdmc:/switch/KosmosToolbox/config.json");

  if (configFile.fail()) {
    Gui::g_nextGui = GUI_MAIN;
    return;
  }

  json configJson;
  try {
    configFile >> configJson;
  } catch(json::parse_error& e) {
    return;
  }

  for (auto sysmodule : configJson["sysmodules"]) {
    try {
      std::stringstream path;
      path << "/atmosphere/titles/" << sysmodule["tid"].get<std::string>() << "/exefs.nsp";

      if (access(path.str().c_str(), F_OK) == -1) continue;
      
      this->m_sysmodules.insert(std::make_pair(sysmodule["tid"].get<std::string>(), (sysModule_t){ sysmodule["name"].get<std::string>(), sysmodule["tid"].get<std::string>(), sysmodule["requires_reboot"].get<bool>() }));
      
      u64 sysmodulePid = 0;
      pmdmntGetTitlePid(&sysmodulePid, std::stoul(sysmodule["tid"].get<std::string>(), nullptr, 16));

      if (sysmodulePid > 0)
        this->m_runningSysmodules.insert(sysmodule["tid"].get<std::string>());
      
    } catch(json::parse_error &e) {
      continue;
    }
  }

  u16 xOffset = 0, yOffset = 0;
  s32 cnt = 0;
  s32 sysmoduleCnt = this->m_sysmodules.size();

  for (auto &sysmodule : this->m_sysmodules) {
    FILE *exefs = fopen(std::string("/atmosphere/titles/" + sysmodule.second.titleID + "/exefs.nsp").c_str(), "r");

    if (exefs == nullptr)
      continue;

    fclose(exefs);

    anyModulesPresent = true;

   new Button(pagexoffset + xOffset, 250 + yOffset, 500, 80, [&](Gui *gui, u16 x, u16 y, bool *isActivated){
      gui->drawTextAligned(font20, x + 37, y + 50, currTheme.textColor, sysmodule.second.name.c_str(), ALIGNED_LEFT);
      gui->drawTextAligned(font20, x + 420, y + 50, this->m_runningSysmodules.find(sysmodule.first) != this->m_runningSysmodules.end() ? currTheme.selectedColor : Gui::makeColor(0xB8, 0xBB, 0xC2, 0xFF), this->m_runningSysmodules.find(sysmodule.first) != this->m_runningSysmodules.end() ? "On" : "Off", ALIGNED_LEFT);
    }, [&](u32 kdown, bool *isActivated){
       if (kdown & KEY_L) 
       {
         currentpage--;
         if(currentpage < 1)
         {
           pagexoffset = -(50 + (pagextooffset * (pagescount + 1) ));
           currentpage = pagescount;
         }
         else
         {
           pagexoffset += 1100;
         }
         Gui::g_nextGui = GUI_SM_SELECT;
       }

       if (kdown & KEY_R){
         currentpage++;
         if(currentpage < pagescount + 1){
           pagexoffset -= 1100;
         }else{
           pagexoffset = 100;
           currentpage = 1;
         }
         Gui::g_nextGui = GUI_SM_SELECT;
        }
      
      if (kdown & KEY_A) {
        u64 pid;
        u64 tid = std::stol(sysmodule.first.c_str(), nullptr, 16);

        std::stringstream path;
        path << "/atmosphere/titles/" << sysmodule.first << "/flags/boot2.flag";


        if (this->m_runningSysmodules.find(sysmodule.first) != this->m_runningSysmodules.end()) {
          if (!sysmodule.second.requiresReboot) {
            pmshellTerminateProcessByTitleId(tid);
          } else {
            (new MessageBox("This sysmodule requires a reboot to fully work. \n Please restart your console in order use it.", MessageBox::OKAY))->show();
          }

          remove(path.str().c_str());
        }
        else {
          if (sysmodule.second.requiresReboot) {
            (new MessageBox("This sysmodule requires a reboot to fully work. \n Please restart your console in order use it.", MessageBox::OKAY))->show();
            FILE *fptr = fopen(path.str().c_str(), "wb+");
            if (fptr != nullptr) fclose(fptr);
          } else {
            if (R_SUCCEEDED(pmshellLaunchProcess(0, tid, FsStorageId_None, &pid))) {
              FILE *fptr = fopen(path.str().c_str(), "wb+");
              if (fptr != nullptr) fclose(fptr);
            }
          }
        }

        pid = 0;
        pmdmntGetTitlePid(&pid, tid);

        if (!sysmodule.second.requiresReboot) {
          if (pid != 0)
            this->m_runningSysmodules.insert(sysmodule.first);
          else
            this->m_runningSysmodules.erase(sysmodule.first);     
        } else {
          if (access(path.str().c_str(), F_OK) == 0)
            this->m_runningSysmodules.insert(sysmodule.first);
          else
            this->m_runningSysmodules.erase(sysmodule.first);    
        }
      }
    }, { (cnt % 3) > 0 ? cnt - 1 : -1, ((cnt % 3) < 3) && (cnt < (sysmoduleCnt - 1)) ? cnt + 1 : -1, cnt >= 3 ? cnt - 3 : -1, (cnt < 3) && (cnt + 3 < sysmoduleCnt) ? cnt + 3 : + (sysmoduleCnt -1) }, false, 
    [&]() -> bool { return true; });
  

    yOffset += 100;

    if (yOffset == 3 * 100) {
      xOffset += 550;
      yOffset = 0;
    }

    cnt++;
  }

}

GuiSysmodule::~GuiSysmodule() {
  pmshellExit();
  pmdmntExit();

  Button::g_buttons.clear();
}

void GuiSysmodule::update() {
  Gui::update();
}

void GuiSysmodule::draw() {
  Gui::beginDraw();
  Gui::drawRectangle(0, 0, Gui::g_framebuffer_width, Gui::g_framebuffer_height, currTheme.backgroundColor);
  Gui::drawRectangle((u32)((Gui::g_framebuffer_width - 1220) / 2), 87, 1220, 1, currTheme.textColor);
  Gui::drawRectangle((u32)((Gui::g_framebuffer_width - 1220) / 2), Gui::g_framebuffer_height - 73, 1220, 1, currTheme.textColor);
  Gui::drawTextAligned(fontIcons, 70, 68, currTheme.textColor, "\uE130", ALIGNED_LEFT);
  Gui::drawTextAligned(font24, 70, 58, currTheme.textColor, "        Kosmos Toolbox", ALIGNED_LEFT);

  if (hidMitmInstalled())
    Gui::drawTextAligned(font20, Gui::g_framebuffer_width - 50, Gui::g_framebuffer_height - 25, currTheme.textColor, "\uE0E2 Key configuration     \uE0E1 Back     \uE0E0 Ok", ALIGNED_RIGHT);
  else
    Gui::drawTextAligned(font20, Gui::g_framebuffer_width - 50, Gui::g_framebuffer_height - 25, currTheme.textColor, "\uE0E4 Page left     \uE0E5 Page right     \uE0E1 Back     \uE0E0 Ok", ALIGNED_RIGHT);

  if (anyModulesPresent)
  {
    Gui::drawTextAligned(font20, Gui::g_framebuffer_width / 2, 150, currTheme.textColor, "Select the background services (sysmodules) that should be running. \n Because of memory restraints it may be not possible to start all services at once.", ALIGNED_CENTER);
    char pagebuffer[100];
    sprintf(pagebuffer, "Page: %d / %d", currentpage, pagescount);
    Gui::drawTextAligned(font20, Gui::g_framebuffer_width / 2, 600, currTheme.textColor, pagebuffer, ALIGNED_CENTER);
  }
  else
  {
    Gui::drawTextAligned(font20, Gui::g_framebuffer_width / 2, 550, currTheme.textColor, "You currently don't have any supported sysmodules installed. To use this \n feature, please install any supported sysmodule as an NSP.", ALIGNED_CENTER);
  }
    

  for(Button *btn : Button::g_buttons)
    btn->draw(this);

  Gui::endDraw();
}

void GuiSysmodule::onInput(u32 kdown) {
  for(Button *btn : Button::g_buttons) {
    if (btn->isSelected())
      if (btn->onInput(kdown)) break;
  }
  
  if (kdown & KEY_B)
    Gui::g_nextGui = GUI_MAIN;

  if (hidMitmInstalled() && kdown & KEY_X)
    Gui::g_nextGui = GUI_HID_MITM;

}

void GuiSysmodule::onTouch(touchPosition &touch) {
  for(Button *btn : Button::g_buttons) {
    btn->onTouch(touch);
  }
}

void GuiSysmodule::onGesture(touchPosition &startPosition, touchPosition &endPosition) {

}
