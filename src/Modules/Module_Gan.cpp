// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
//
// -----------------------------------------------------------------------
//
// This file is part of RLVM, a RealLive virtual machine clone.
//
// -----------------------------------------------------------------------
//
// Copyright (C) 2007 Elliot Glaysher
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//
// -----------------------------------------------------------------------

#include "Modules/Module_Gan.hpp"

#include "MachineBase/LongOperation.hpp"
#include "MachineBase/Properties.hpp"
#include "MachineBase/RLMachine.hpp"
#include "MachineBase/RLModule.hpp"
#include "MachineBase/RLOperation.hpp"
#include "MachineBase/RLOperation/RLOp_Store.hpp"
#include "Modules/Module_Obj.hpp"
#include "Systems/Base/GanGraphicsObjectData.hpp"
#include "Systems/Base/GraphicsObject.hpp"
#include "Systems/Base/GraphicsSystem.hpp"
#include "Systems/Base/ParentGraphicsObjectData.hpp"
#include "Systems/Base/System.hpp"

#include <boost/shared_ptr.hpp>

using namespace std;
using namespace boost;
using namespace libReallive;

// -----------------------------------------------------------------------

namespace {

struct objWaitAll : public RLOp_Void_Void {
  struct WaitUntilAllDone : public LongOperation {
    bool operator()(RLMachine& machine) {
      // Clannad puts us in DrawManual() right before calling us so we force
      // refreshes.
      machine.system().graphics().forceRefresh();
      return !machine.system().graphics().animationsPlaying();
    }
  };

  void operator()(RLMachine& machine) {
    machine.pushLongOperation(new WaitUntilAllDone);
  }
};

struct ganPlay : public RLOp_Void_2<IntConstant_T, IntConstant_T> {
  struct WaitForGanToFinish : public LongOperation {
    /**
     * Save the screen update mode and change it to automatic when entering a
     * blocking animation, restoring when we leave.
     *
     * When I actually started paying attention to DrawManual()/DrawAuto(), I
     * stopped forcing a refresh of the screen on each and instead obeyed the
     * DCScreenUpdateMode, I broke the OP of Kanon.
     *
     * I don't know if this is the correct solution. If this breaks things, the
     * solution is that things like AnmGraphicsObjectData force screen
     * refreshes, ignoring whatever the current screen update mode is.
     */
    GraphicsSystem& system_;
    GraphicsSystem::DCScreenUpdateMode mode_;

    int fgbg_;
    int parent_;
    int buf_;
    WaitForGanToFinish(GraphicsSystem& system, int fgbg, int parent,
                       int inBuf)
      : system_(system),
        mode_(system_.screenUpdateMode()),
        fgbg_(fgbg), parent_(parent), buf_(inBuf) {
      system_.setScreenUpdateMode(GraphicsSystem::SCREENUPDATEMODE_AUTOMATIC);
    }

    bool operator()(RLMachine& machine) {
      GraphicsObject& obj = getObject(machine);
      bool done = true;

      if (obj.hasObjectData()) {
        const GraphicsObjectData& data = obj.objectData();
        if (data.isAnimation())
          done = !data.currentlyPlaying();
      }

      if (done) {
        // Restore whatever mode we were in before.
        system_.setScreenUpdateMode(mode_);
      }

      return done;
    }

    GraphicsObject& getObject(RLMachine& machine) {
      GraphicsSystem& graphics = machine.system().graphics();

      if (parent_ != -1) {
        GraphicsObject& parent = graphics.getObject(fgbg_, parent_);
        ensureIsParentObject(parent, graphics.objectLayerSize());
        return static_cast<ParentGraphicsObjectData&>(parent.objectData()).
            getObject(buf_);
      } else {
        return graphics.getObject(fgbg_, buf_);
      }
    }
  };

  bool block_;
  GraphicsObjectData::AfterAnimation after_effect_;

  ganPlay(bool block, GraphicsObjectData::AfterAnimation after)
    : block_(block), after_effect_(after) {}

  void operator()(RLMachine& machine, int buf, int animationSet) {
    GraphicsObject& obj = getGraphicsObject(machine, this, buf);

    if (obj.hasObjectData()) {
      GraphicsObjectData& data = obj.objectData();
      if (data.isAnimation()) {
        data.playSet(animationSet);
        data.setAfterAction(after_effect_);

        if (block_) {
          int fgbg;
          if (!getProperty(P_FGBG, fgbg))
            fgbg = OBJ_FG;

          int parent_object;
          if (!getProperty(P_PARENTOBJ, parent_object))
            parent_object = -1;

          machine.pushLongOperation(new WaitForGanToFinish(
                                      machine.system().graphics(),
                                      fgbg, parent_object, buf));
        }
      }
    }
  }
};

/**
 * Returns true when an animation is completed.
 *
 * RATIONALE:
 *
 * This is a guess. I'm not entirely sure what this function does, but I'm
 * trying to pick the meaning out of context. Here's a snippet of CLANNAD's
 * SEEN9081, where it's used:
 *
 * @code
 * #line 412
 * ganPlay2 (3, 15)
 *
 * [...]
 *
 *   @62
 * #line 416
 * op<1:073:00003, 0> (3)
 * intA[0] = store
 * #line 417
 * goto_unless (!intA[0]) @63
 * #line 418
 * intA[200] = 19
 * #line 420
 *
 *   @63
 * goto @64
 * @endcode
 *
 * So it looks like it's trying to set some sort of sentinel value since
 * intA[200] is checked later. It's checking SOMETHING about the gan object,
 * and I'm assuming it's whether it's done animating.
 *
 * After implementing this, we no longer get stuck in an infinite loop during
 * Ushio's birth so I'm assuming this is correct.
 */
struct isGanDonePlaying : public RLOp_Store_1<IntConstant_T> {
  int operator()(RLMachine& machine, int gan_num) {
    GraphicsObject& obj = getGraphicsObject(machine, this, gan_num);

    if (obj.hasObjectData()) {
      GraphicsObjectData& data = obj.objectData();
      if (data.isAnimation() && data.animationFinished())
        return 1;
    }

    return 0;
  }
};

struct objStop_0 : public RLOp_Void_1<IntConstant_T> {
  void operator()(RLMachine& machine, int obj_num) {
    GraphicsObject& obj = getGraphicsObject(machine, this, obj_num);
    if (obj.hasObjectData())
      obj.objectData().setCurrentlyPlaying(false);
  }
};

}  // namespace

// -----------------------------------------------------------------------

void addGanOperationsTo(RLModule& m) {
  m.addOpcode(0, 0, "objStop2", new objStop_0);
  m.addOpcode(3, 0, "ganIsDonePlaying", new isGanDonePlaying);

  m.addOpcode(1000, 0, "objStop", new objStop_0);
  m.addUnsupportedOpcode(1000, 1, "objStop");

  m.addOpcode(104, 0, "objWaitAll", new objWaitAll);

  m.addOpcode(1001, 0, "ganLoop",
              new ganPlay(false, GraphicsObjectData::AFTER_LOOP));
  m.addOpcode(1003, 0, "ganPlay",
              new ganPlay(false, GraphicsObjectData::AFTER_NONE));
  m.addOpcode(1005, 0, "ganPlayOnce",
              new ganPlay(false, GraphicsObjectData::AFTER_CLEAR));
  m.addOpcode(1006, 0, "ganPlayEx",
              new ganPlay(true, GraphicsObjectData::AFTER_NONE));
  m.addOpcode(1007, 0, "ganPlayOnceEx",
              new ganPlay(true, GraphicsObjectData::AFTER_CLEAR));

  m.addOpcode(2001, 0, "objLoop",
              new ganPlay(false, GraphicsObjectData::AFTER_LOOP));
  m.addUnsupportedOpcode(2003, 0, "objPlay");

  m.addOpcode(3001, 0, "ganLoop2",
              new ganPlay(false, GraphicsObjectData::AFTER_LOOP));
  m.addOpcode(3003, 0, "ganPlay2",
              new ganPlay(false, GraphicsObjectData::AFTER_NONE));
  m.addOpcode(3005, 0, "ganPlayOnce2",
              new ganPlay(false, GraphicsObjectData::AFTER_CLEAR));
  m.addOpcode(3006, 0, "ganPlayEx2",
              new ganPlay(true, GraphicsObjectData::AFTER_NONE));
  m.addOpcode(3007, 0, "ganPlayOnceEx2",
              new ganPlay(true, GraphicsObjectData::AFTER_CLEAR));

  m.addOpcode(3103, 0, "ganPlay3",
              new ganPlay(false, GraphicsObjectData::AFTER_NONE));
}

// -----------------------------------------------------------------------

GanFgModule::GanFgModule()
    : RLModule("GanFg", 1, 73) {
  addGanOperationsTo(*this);
  setProperty(P_FGBG, OBJ_FG);
}

// -----------------------------------------------------------------------

GanBgModule::GanBgModule()
    : RLModule("GanBg", 1, 74) {
  addGanOperationsTo(*this);
  setProperty(P_FGBG, OBJ_BG);
}

// -----------------------------------------------------------------------

ChildGanFgModule::ChildGanFgModule()
    : MappedRLModule(childObjMappingFun, "ChildGanFg", 2, 73) {
  addGanOperationsTo(*this);
  setProperty(P_FGBG, OBJ_FG);
}


// -----------------------------------------------------------------------

ChildGanBgModule::ChildGanBgModule()
    : MappedRLModule(childObjMappingFun, "ChildGanBg", 2, 74) {
  addGanOperationsTo(*this);
  setProperty(P_FGBG, OBJ_BG);
}
