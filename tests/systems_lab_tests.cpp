// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor/campaign.h"
#include "editor/runtime.h"
#include "headless_system.h"
#include "config.h"
#include "game.h"
#include "third_party/editor/json.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
Options g_options;
const char *g_caption = "Systems Lab walkthrough";
static void check(bool ok, const std::string &why) {
    if (!ok) throw std::runtime_error(why);
}
int main(int argc, char **argv) {
    try {
        using namespace editor;
        if (argc != 4) return 2;
        setDefaultOptions(g_options);
        const auto root = std::filesystem::absolute(argv[1]);
        const auto out = std::filesystem::absolute(argv[3]);
        std::filesystem::create_directories(out);
        const auto library = assets::discover(argv[2]);
        check(!library.games.empty(), "Missing original assets");
        auto p = loadProject(root / "project.r2project.json");
        check(p.rooms.size() == 5, "Expected exactly five rooms");
        auto campaign = loadCampaign(root / "campaign.r2campaign.json");
        check(validateCampaign(campaign, root, true).empty(), "Invalid campaign");
        HeadlessSystem stub;
        auto scene = std::make_unique<Scene>(p, root, library.games.front(), &stub);
        auto effectActive = [&](size_t module, const char *id) {
            const int index = nodeIndex(p.modules[module], id);
            check(index >= 0, std::string("Missing lab effect: ") + id);
            return scene->states()[module].graph.nodes[index].active;
        };
        std::filesystem::path captureRoute;
        int routeTicks=0,routeFrames=0;
        auto tick = [&] {
            scene->tick();
            if(!captureRoute.empty() && ++routeTicks%2==0) {
                char file[32];std::snprintf(file,sizeof(file),"frame-%03d.png",routeFrames++);
                writePNG(scene->render(),captureRoute/file);
            }
            const auto &airlock = scene->states()[2].graph.nodes;
            check(!(airlock[0].active && airlock[1].active), "Both airlock doors opened");
            check(!scene->dead(), "Conrad died during the safe training route");
        };
        auto idle = [&](int count) {
            stub._pi = {};
            while (count--) tick();
        };
        auto talk = [&] {
            for (int i=0; i<180 && scene->dialogueOpen(); ++i) {
                stub._pi = {};
                stub._pi.enter = (i % 2) == 1;
                tick();
            }
            idle(1);
            check(!scene->dialogueOpen(), "Dialogue did not finish");
        };
        auto cancel = [&] {
            for (int t=0;t<180;++t) {
                const auto state=nlohmann::json::parse(scene->saveSession());
                if (state["page"].get<size_t>()+1==state["dialogue"].size()) break;
                stub._pi.enter=true; tick(); idle(1);
            }
            stub._pi.dirMask=PlayerInput::DIR_DOWN; tick(); idle(1);
            stub._pi.enter=true; tick(); idle(1);
            check(!scene->dialogueOpen(), "Log out failed to close dialogue");
        };
        // Room-list links are rebuilt in actor-index order by Game::loadState.
        // Compare all gameplay data except those derived serialization links.
        auto normalized = [&](const std::string &snapshot) {
            auto state=nlohmann::json::parse(snapshot);
            auto bytes=state["game"].get<std::string>();
            for(int i=0;i<scene->game()._res._pgeNum;++i)
                bytes.replace((38+13+i*30+22)*2,8,"00000000");
            state["game"]=bytes;
            return state;
        };
        auto walk = [&](int room, int x) {
            for (int i=0; i<1400; ++i) {
                if (!scene->exitRoute().empty() ||
                    (scene->currentRoom()==room && scene->game()._pgeLive[0].pos_x>=x)) {
                    stub._pi={}; return;
                }
                stub._pi={}; stub._pi.dirMask=PlayerInput::DIR_RIGHT;
                tick();
                if (scene->dialogueOpen()) talk();
            }
            const auto &a=scene->game()._pgeLive[0];
            throw std::runtime_error("Blocked walking to room="+std::to_string(room)+" x="+std::to_string(x)+
                                     "; at room="+std::to_string(scene->currentRoom())+" x="+std::to_string(a.pos_x)+" y="+std::to_string(a.pos_y));
        };
        idle(1); writePNG(scene->render(),out/"00-opening.png"); talk(); idle(20);
        check(p.modules.size()==13, "Five room ambience modules are missing");
        for(size_t m=8;m<p.modules.size();++m)
            for(size_t n=1;n<p.modules[m].nodes.size();++n)
                if(p.modules[m].nodes[n].id=="lamp" || p.modules[m].nodes[n].id=="light" ||
                   p.modules[m].nodes[n].id=="dust" || p.modules[m].nodes[n].id=="fan" ||
                   p.modules[m].nodes[n].id=="monitor" || p.modules[m].nodes[n].id=="scrubber-fan" ||
                   p.modules[m].nodes[n].id=="battery-status" ||
                   p.modules[m].nodes[n].id=="vent-drift" ||
                   p.modules[m].nodes[n].id=="lit-dust")
                    check(scene->states()[m].graph.nodes[n].active, "A room service did not start");
        const auto ambientFrame=scene->render();
        const auto ambientSave=scene->saveSession();
        idle(17); scene->restoreSession(ambientSave);
        check(scene->render().pixels==ambientFrame.pixels, "Lab ambience phase changed after save/load");
        writePNG(scene->render(),out/"01-movement.png");
        const int life=scene->game()._pgeLive[0].life;
        walk(1,40); idle(20);
        writePNG(scene->render(),out/"02-shooting.png");
        const int a=scene->states()[1].actors[1];
        for (int t=0;t<500;++t) {
            stub._pi={}; stub._pi.space=t==20; stub._pi.shift=t>=45;
            tick();
            if (scene->game()._pgeLive[a].life<=0) break;
        }
        check(scene->game()._pgeLive[a].life==0,
              "Native pistol did not destroy the training droid");
        check(scene->states()[1].graph.nodes[2].count==1, "Target counter did not count the droid death");
        check(scene->game()._pgeLive[0].life==life, "Held training targets retaliated");
        talk(); idle(2);
        stub._pi.space=true; tick(); idle(40);
        walk(1,192); idle(20);
        check(scene->states()[6].graph.nodes[4].active && scene->states()[6].graph.nodes[5].completed &&
              !scene->states()[6].graph.nodes[6].enabled, "Approach failed to open the gallery door");
        check(scene->currentRoom()==1, "Approach unexpectedly teleported Conrad");
        writePNG(scene->render(),out/"13-gallery-door.png");
        stub._pi.enter=true; tick(); idle(1); talk(); idle(20);
        check(scene->currentRoom()==4, "Gallery doorway failed");
        check(!scene->states()[6].graph.nodes[4].active && scene->states()[6].graph.nodes[6].completed,
              "Leaving the room failed to close the old doorway");
        check(scene->states()[6].graph.nodes[7].active && scene->states()[6].graph.nodes[8].completed,
              "Arriving beside the return doorway did not open it");
        writePNG(scene->render(),out/"14-gallery.png");
        const auto movingSave=scene->saveSession();
        idle(31); scene->restoreSession(movingSave);
        check(normalized(scene->saveSession())==normalized(movingSave), "Moving target save lost state");
        int hitCount=0, shotCount=0, lastFrame=-1;
        uint64_t lightGeneration=scene->states()[7].graph.nodes[2].generation;
        for (int t=0;t<500;++t) {
            stub._pi={}; stub._pi.space=t==20; stub._pi.shift=t>=45;
            tick();
            auto &g=scene->game();
            const int effect=g._res._pgeInit[g._res._pgeInit[0].data[0]].data[0];
            const int frame=g._pgeLive[effect].anim_number;
            if(frame==287 && lastFrame!=287) ++shotCount;
            lastFrame=frame;
            const auto &light=scene->states()[7].graph.nodes[2];
            if(light.generation!=lightGeneration) {
                lightGeneration=light.generation; ++hitCount;
                check(light.active, "Target hit left lamp dark");
                if(hitCount==2) writePNG(scene->render(),out/"15-target-green-hit.png");
            }
        }
        check(hitCount>=3 && hitCount<shotCount, "Moving target did not distinguish hits and misses");
        idle(40);
        check(!scene->states()[7].graph.nodes[2].active, "Green lamp did not expire");
        writePNG(scene->render(),out/"16-target-light-off.png");
        stub._pi.space=true; tick(); idle(35);
        for(int t=0;t<100 && scene->game()._pgeLive[0].pos_x>20;++t) {
            stub._pi={}; stub._pi.dirMask=PlayerInput::DIR_LEFT; tick();
        }
        idle(20); stub._pi.enter=true; tick(); idle(20);
        check(scene->currentRoom()==1, "Gallery return door failed");
        // Return facing right before continuing through the main lab.
        walk(2,p.modules[3].nodes[0].at.x+8); idle(20);
        check(scene->states()[5].graph.nodes[1].active && scene->states()[5].graph.nodes[2].active,
              "Background experiments never started");
        check(effectActive(5,"sample-motes"), "Sample display started without its motes");
        writePNG(scene->render(),out/"03-workshop.png");
        stub._pi.enter=true; tick(); idle(1);
        check(scene->dialogueOpen(), "Repair console did not open");
        cancel();
        check(!scene->states()[3].graph.nodes[0].active && !scene->states()[3].graph.nodes[3].active,
              "Cancelled diagnostics changed the android");
        check(!effectActive(3,"scan-motes"), "Cancelled diagnostics emitted particles");
        stub._pi.enter=true; tick(); idle(1); talk();
        check(scene->states()[3].graph.nodes[3].active, "Diagnostic scan did not start");
        check(effectActive(3,"scan-motes"), "Diagnostic scan did not start its particles");
        writePNG(scene->render(),out/"04-diagnostics.png");
        idle(15);
        const auto repairSave=scene->saveSession();
        idle(10); scene->restoreSession(repairSave);
        check(normalized(scene->saveSession())==normalized(repairSave), "Diagnostic save lost state");
        idle(100); talk();
        check(scene->states()[3].graph.nodes[4].active && scene->states()[3].graph.nodes[0].completed &&
              !scene->states()[3].graph.nodes[3].active, "Android did not finish its diagnostic sweep");
        idle(2);stub._pi.enter=true;tick();idle(1);
        check(scene->dialogueOpen(),"Completed repair console stopped responding");talk();
        walk(2,p.modules[4].nodes[0].at.x+8); idle(20);
        stub._pi.enter=true; tick(); idle(1);
        check(scene->dialogueOpen(), "Laser console did not open");
        cancel();
        check(!scene->states()[4].graph.nodes[0].active && !scene->states()[4].graph.nodes[3].active,
              "Cancelled laser test fired a pulse");
        stub._pi.enter=true; tick(); idle(1); talk();
        check(scene->states()[4].graph.nodes[3].active, "Laser pulse did not start");
        writePNG(scene->render(),out/"05-laser-test.png");
        const auto laserSave=scene->saveSession();
        idle(10); scene->restoreSession(laserSave);
        check(normalized(scene->saveSession())==normalized(laserSave), "Laser save lost pulse timing");
        idle(70); talk();
        check(!scene->states()[4].graph.nodes[3].active && !scene->states()[4].graph.nodes[0].active,
              "Laser pulse or console failed to reset");
        check(!effectActive(4,"receiver-sparks"), "Receiver discharge outlived the laser test");
        stub._pi.enter=true; tick(); idle(1); talk();
        check(scene->states()[4].graph.nodes[3].active, "Laser pulse could not be repeated");
        check(effectActive(4,"receiver-sparks"), "Repeated laser pulse lost its discharge");
        check(scene->game()._pgeLive[0].life==life, "Lab experiment hurt Conrad");
        walk(3,128); idle(20);
        check(!scene->states()[4].graph.nodes[3].active, "Laser did not shut down after leaving the room");
        writePNG(scene->render(),out/"06-airlock.png");
        stub._pi.enter=true; tick(); idle(1);
        check(scene->dialogueOpen(), "Terminal did not open the access dialogue");
        writePNG(scene->render(),out/"07-terminal.png");
        // Log out without changing either hatch, then reopen the terminal.
        cancel();
        check(!scene->dialogueOpen() && !scene->states()[2].graph.nodes[2].active &&
              scene->states()[2].graph.nodes[0].active && !scene->states()[2].graph.nodes[1].active,
              "Logging out unexpectedly changed the airlock");
        stub._pi.enter=true; tick(); idle(1);
        check(scene->dialogueOpen(), "Terminal could not reopen after logging out");
        talk();
        check(!scene->states()[2].graph.nodes[0].active && !scene->states()[2].graph.nodes[1].active,
              "Pressure cycle did not close the inner door first");
        check(scene->states()[2].graph.nodes.back().active, "Pressure vent did not start mist");
        idle(30);
        const auto saved=scene->saveSession();
        idle(15);
        scene->restoreSession(saved);
        check(normalized(scene->saveSession())==normalized(saved),
              "Mid-cycle save did not restore gameplay state");
        writePNG(scene->render(),out/"08-pressure-cycle.png");
        idle(95); talk();
        check(scene->states()[2].graph.nodes[1].active, "Outer door stayed locked after pressure cycle");
        check(!scene->states()[2].graph.nodes.back().active, "Pressure vent continued after equalization");
        writePNG(scene->render(),out/"09-exit-open.png");
        idle(2);stub._pi.enter=true;tick();idle(1);
        check(scene->dialogueOpen(),"Completed airlock terminal stopped responding");talk();
        walk(3,239);
        check(scene->exitRoute()=="complete", "Departure threshold failed");
        Images images; images.root=root;
        writePNG(renderStory(campaign.stages.back(),images,10000,true),out/"10-debrief.png");
        scene.reset();
        // Check both optional ledge climbs using unmodified native Up input.
        for (int shelf=0;shelf<2;++shelf) {
            auto climb=p;
            climb.spawn.x=shelf?128:32;
            climb.spawn.y=shelf?142:214;
            climb.modules[0].nodes[0].enabled=false;
            stub._pi={};
            scene=std::make_unique<Scene>(climb,root,library.games.front(),&stub);
            idle(20);
            for(int t=0;t<150;++t) {
                stub._pi={};stub._pi.dirMask=t<20?PlayerInput::DIR_RIGHT:PlayerInput::DIR_UP;tick();
            }
            const auto &actor=scene->game()._pgeLive[0];
            check(actor.pos_y==(shelf?70:142), "Ledge climb failed: shelf="+std::to_string(shelf)+" x="+std::to_string(actor.pos_x)+" y="+std::to_string(actor.pos_y));
            writePNG(scene->render(),out/(shelf?"12-upper-platform.png":"11-first-ledge.png"));
            scene.reset();
        }
        // The new artwork's decks are real geometry, reached by native climbing.
        for (int room : {1,2,3,4}) {
            auto climb=p;
            climb.spawn={climb.rooms[room].id,32,214};
            for(auto &module:climb.modules) for(auto &n:module.nodes)
                if(n.kind==NodeKind::Dialogue) n.enabled=false;
            stub._pi={};
            scene=std::make_unique<Scene>(climb,root,library.games.front(),&stub);
            idle(20);
            for(int t=0;t<150;++t) {
                stub._pi={}; stub._pi.dirMask=t<20?PlayerInput::DIR_RIGHT:PlayerInput::DIR_UP; tick();
            }
            const auto &actor=scene->game()._pgeLive[0];
            check(scene->currentRoom()==room && actor.pos_y==142,
                  "New upper deck could not be climbed: room="+std::to_string(room)+
                  " x="+std::to_string(actor.pos_x)+" y="+std::to_string(actor.pos_y));
            writePNG(scene->render(),out/(room==1?"17-calibration-upper.png":room==2?"18-workshop-upper.png":"19-gallery-upper.png"));
            if(room==4) {
                int hits=0;
                auto generation=scene->states()[7].graph.nodes[2].generation;
                for(int t=0;t<500;++t) {
                    stub._pi={}; stub._pi.space=t==20; stub._pi.shift=t>=45; tick();
                    const auto &light=scene->states()[7].graph.nodes[2];
                    if(light.generation!=generation) {
                        generation=light.generation; ++hits;
                        if(hits==2) writePNG(scene->render(),out/"20-gallery-upper-hit.png");
                    }
                }
                check(hits>=2, "Upper firing deck could not hit the moving plate");
                idle(20); stub._pi.space=true; tick(); idle(30);
            }
            for(int t=0;t<150 && scene->game()._pgeLive[0].pos_x>=48;++t) {
                stub._pi={}; stub._pi.dirMask=PlayerInput::DIR_LEFT; tick();
            }
            idle(80);
            check(scene->currentRoom()==room && scene->game()._pgeLive[0].pos_y==214,
                  "Upper deck did not permit a safe return to the lower floor");
            scene.reset();
        }
        // Reach all eight top rooms from the lower floor with native movement.
        // Return through the central gap, then the left shaft, without damage.
        for(int room : {1,2,3,4}) for(bool left : {false,true}) {
            auto climb=p;
            climb.spawn={climb.rooms[room].id,32,214};
            for(auto &m:climb.modules) for(auto &n:m.nodes)
                if(n.kind==NodeKind::Dialogue) n.enabled=false;
            stub._pi={}; scene=std::make_unique<Scene>(climb,root,library.games.front(),&stub);
            if(std::getenv("R2_LAB_CAPTURE") && room==1 && !left) {
                captureRoute=out/"motion-upper-route"; std::filesystem::create_directories(captureRoute);
                routeTicks=routeFrames=0;
            }
            idle(20);
            for(int t=0;t<150;++t) {
                stub._pi={}; stub._pi.dirMask=t<20?PlayerInput::DIR_RIGHT:PlayerInput::DIR_UP; tick();
            }
            check(scene->game()._pgeLive[0].pos_y==142,"Middle access failed");
            walk(room,128); idle(20);
            for(int t=0;t<150;++t) {
                stub._pi={}; stub._pi.dirMask=t<(left?44:20)?(left?PlayerInput::DIR_LEFT:PlayerInput::DIR_RIGHT):PlayerInput::DIR_UP; tick();
            }
            const auto &a=scene->game()._pgeLive[0];
            check(a.pos_y==70,"Top cabin unreachable room="+std::to_string(room)+" left="+std::to_string(left)+
                  " x="+std::to_string(a.pos_x)+" y="+std::to_string(a.pos_y));
            for(int t=0;t<20;++t) { stub._pi={};stub._pi.dirMask=left?PlayerInput::DIR_LEFT:PlayerInput::DIR_RIGHT;tick(); }
            idle(20);
            check(a.pos_y==70 && a.pos_x>=16 && a.pos_x<224,"Top cabin walk escaped its hull");
            if(room==3) check(a.pos_x<208 && scene->exitRoute().empty(),"Top gallery bypassed airlock bulkhead");
            writePNG(scene->render(),out/("top-room-"+std::to_string(room+1)+(left?"-left.png":"-right.png")));
            for(int t=0;t<180 && (left?a.pos_x<112:a.pos_x>128);++t) {
                stub._pi={};stub._pi.dirMask=left?PlayerInput::DIR_RIGHT:PlayerInput::DIR_LEFT;tick();
            }
            idle(80);
            check(a.pos_y==142,"Top cabin had no safe central return drop");
            for(int t=0;t<150 && a.pos_x>=48;++t) {stub._pi={};stub._pi.dirMask=PlayerInput::DIR_LEFT;tick();}
            idle(80);
            check(a.pos_y==214,"Top cabin had no safe return to ground");
            captureRoute.clear();
            scene.reset();
        }
        // Proximity animation state must survive a save made while opening.
        for(int room : {1,4}) {
            auto door=p; door.spawn={door.rooms[room].id,room==1?140:84,214};
            for(auto &m:door.modules) for(auto &n:m.nodes)
                if(n.kind==NodeKind::Dialogue) n.enabled=false;
            stub._pi={};scene=std::make_unique<Scene>(door,root,library.games.front(),&stub);
            idle(20);
            const int sensor=room==1?4:7,opening=sensor+1,closing=sensor+2;
            for(int t=0;t<80 && !scene->states()[6].graph.nodes[sensor].active;++t) {
                stub._pi={};stub._pi.dirMask=room==1?PlayerInput::DIR_RIGHT:PlayerInput::DIR_LEFT;tick();
            }
            idle(2);
            check(scene->states()[6].graph.nodes[opening].active,"Door failed to begin opening");
            const auto saved=scene->saveSession(); const auto pixels=scene->render().pixels;
            idle(30);scene->restoreSession(saved);
            check(normalized(saved)==normalized(scene->saveSession()) && pixels==scene->render().pixels,
                  "Door mid-animation save changed its phase or occupancy");
            idle(30);
            for(int t=0;t<80 && scene->states()[6].graph.nodes[sensor].active;++t) {
                stub._pi={};stub._pi.dirMask=room==1?PlayerInput::DIR_LEFT:PlayerInput::DIR_RIGHT;tick();
            }
            idle(2);
            check(scene->states()[6].graph.nodes[closing].active,"Door did not close on departure");
            for(int t=0;t<80 && !scene->states()[6].graph.nodes[sensor].active;++t) {
                stub._pi={};stub._pi.dirMask=room==1?PlayerInput::DIR_RIGHT:PlayerInput::DIR_LEFT;tick();
            }
            idle(25);
            check(scene->states()[6].graph.nodes[opening].completed && !scene->states()[6].graph.nodes[closing].enabled,
                  "Reapproaching a closing door left conflicting animations");
            check(scene->currentRoom()==room,"Proximity incorrectly auto-travelled");
            scene.reset();
        }
        auto overhead=p; overhead.spawn={overhead.rooms[1].id,192,142};
        for(auto &m:overhead.modules) for(auto &n:m.nodes) if(n.kind==NodeKind::Dialogue)n.enabled=false;
        stub._pi={};scene=std::make_unique<Scene>(overhead,root,library.games.front(),&stub);idle(30);
        check(!scene->states()[6].graph.nodes[4].active,"Overhead walkway opened the lower door");
        scene.reset();
        std::ifstream manifestFile(root/"interaction-tests.json");
        nlohmann::json controls;manifestFile>>controls;
        int visited=0;
        for(const auto &entry:controls) {
            const std::string moduleId=entry.at("module"),buttonId=entry.at("button");
            const auto m=std::find_if(p.modules.begin(),p.modules.end(),[&](const Module &v){return v.id==moduleId;})-p.modules.begin();
            const int b=nodeIndex(p.modules[m],buttonId);
            check(b>=0,"Missing registered control");
            const auto &button=p.modules[m].nodes[b];
            const std::string mode=entry.at("mode");
            auto fixture=p; fixture.spawn={button.at.room,((button.at.x+button.width/2)/16)*16,button.at.y-2};
            for(auto &mod:fixture.modules)for(auto &n:mod.nodes)if(n.kind==NodeKind::Dialogue)n.enabled=false;
            const int d=entry.contains("dialogue")?nodeIndex(fixture.modules[m],entry["dialogue"]):-1;
            if(d>=0)fixture.modules[m].nodes[d].enabled=true;
            stub._pi={};scene=std::make_unique<Scene>(fixture,root,library.games.front(),&stub);idle(20);
            if(entry.value("requiresDeath",false)) {
                const int actor=scene->states()[1].actors[1];scene->game()._pgeLive[actor].life=0;idle(30);
            }
            auto press=[&] {idle(1);stub._pi.enter=true;tick();idle(1);};
            auto effects=[&](bool expected) {
                for(const auto &id:entry.at("effects")) {
                    const int n=nodeIndex(p.modules[m],id.get<std::string>());
                    check(scene->states()[m].graph.nodes[n].active==expected,
                          buttonId+" failed to set "+id.get<std::string>()+" to "+std::to_string(expected));
                }
            };
            const bool recordSwitch=std::getenv("R2_LAB_CAPTURE") && moduleId=="ambience-1" && mode=="toggle";
            if(recordSwitch) {
                captureRoute=out/("motion-switch-"+buttonId);std::filesystem::create_directories(captureRoute);
                routeTicks=routeFrames=0;idle(30);
            }
            press();
            check(scene->states()[m].graph.nodes[b].completed,"Wrong nearby device selected: "+moduleId+"/"+buttonId+" x="+std::to_string(scene->game()._pgeLive[0].pos_x)+" y="+std::to_string(scene->game()._pgeLive[0].pos_y));
            if(mode=="toggle") {
                effects(false);
                if(recordSwitch)idle(45);
                writePNG(scene->render(),out/("control-"+buttonId+"-off.png"));
                const auto saved=scene->saveSession();idle(11);scene->restoreSession(saved);
                check(normalized(saved)==normalized(scene->saveSession()),"Switch state failed save/load: "+buttonId);
                press();effects(true);
                if(recordSwitch)idle(45);
                check(scene->states()[m].graph.nodes[nodeIndex(p.modules[m],entry.at("alternate"))].completed,
                      "Switch failed to return to its paired input: "+buttonId);
                press();effects(false);press();effects(true);
            } else {
                check(d>=0 && scene->dialogueOpen() && scene->states()[m].graph.nodes[d].active,
                      "Device did not display its own dialogue: "+buttonId);
                writePNG(scene->render(),out/("dialogue-"+buttonId+".png"));
                if(mode=="pulse" || mode=="choice") {
                    cancel(); // choice 2: either cancel a test or put a display in standby
                    effects(false);
                    press();talk();effects(true);
                    const auto saved=scene->saveSession();const auto image=scene->render();
                    idle(7);scene->restoreSession(saved);
                    check(normalized(saved)==normalized(scene->saveSession()) && image.pixels==scene->render().pixels,
                          "Device lost exact saved phase: "+buttonId);
                    if(mode=="pulse") {
                        idle(110);effects(false);press();talk();effects(true);idle(110);effects(false);
                    } else {press();cancel();effects(false);press();talk();effects(true);}
                } else if(mode=="target") {
                    talk();effects(false);
                    const int phase=scene->states()[m].graph.nodes[0].targetPhase;idle(40);
                    check(phase==scene->states()[m].graph.nodes[0].targetPhase,"Paused plate kept moving");
                    press();cancel();effects(true);
                    const int resumed=scene->states()[m].graph.nodes[0].targetPhase;
                    check((resumed-phase+180)%180<=3,"Resuming the plate jumped to a different position");
                } else {talk();press();check(scene->dialogueOpen(),"Read-only computer/droid could not be revisited");talk();}
            }
            check(!scene->dead() && scene->game()._pgeLive[0].pos_y==button.at.y-2,
                  "Device anchor was not on a stable reachable floor: "+buttonId);
            captureRoute.clear();
            ++visited;
            writePNG(scene->render(),out/("control-"+buttonId+".png"));
            scene.reset();
        }
        std::cout<<visited<<" authored controls passed native Enter, choices, repeated use, cancellation and save/load checks\n";
        if(std::getenv("R2_LAB_CAPTURE")) {
            auto demo=p; demo.spawn={demo.rooms[1].id,140,214};
            for(auto &m:demo.modules) for(auto &n:m.nodes) if(n.kind==NodeKind::Dialogue)n.enabled=false;
            stub._pi={};scene=std::make_unique<Scene>(demo,root,library.games.front(),&stub);idle(20);
            captureRoute=out/"motion-door";std::filesystem::create_directories(captureRoute);
            routeTicks=routeFrames=0;
            for(int t=0;t<240;++t) {
                stub._pi={};
                if(t<30 || (t>=140 && t<180))stub._pi.dirMask=PlayerInput::DIR_RIGHT;
                if(t>=70 && t<110)stub._pi.dirMask=PlayerInput::DIR_LEFT;
                tick();
            }
            captureRoute.clear();scene.reset();
            const int starts[]={24,40,120,128,48};
            for(int room=0;room<5;++room) {
                auto preview=p;
                preview.spawn={preview.rooms[room].id,starts[room],214};
                for(auto &module:preview.modules) for(auto &n:module.nodes)
                    if(n.kind==NodeKind::Dialogue) n.enabled=false;
                // Use the real terminal/choice path to start the airlock puff;
                // hide only incidental dialogue in the visual capture fixture.
                if(room==3) preview.modules[2].nodes[3].enabled=true;
                stub._pi={};
                scene=std::make_unique<Scene>(preview,root,library.games.front(),&stub);
                idle(20);
                if(room==3) { stub._pi.enter=true; tick(); idle(1); talk(); }
                const auto frames=out/("motion-room-"+std::to_string(room+1));
                std::filesystem::create_directories(frames);
                for(int frame=0;frame<90;++frame) {
                    char name[32]; std::snprintf(name,sizeof(name),"frame-%03d.png",frame);
                    writePNG(scene->render(),frames/name); idle(2);
                }
                scene.reset();
            }
        }
        std::cout<<"Systems Lab: five-room walkthrough with gallery door, moving plate, per-hit lamp, native shooting, both original ledges, four middle decks, eight accessible top cabins, safe drops, approach/departure doors with save/restore and upper-gallery shooting, diagnostic cancellation/completion, repeatable laser pulse, experiment save/restore, airlock, exit, ambience/save phase and pressure mist passed\n";
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
