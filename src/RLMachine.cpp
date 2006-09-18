// Some code ripped from Jagarl's xclanand. All GPL, right?

#include "defs.h"

#include "archive.h"
#include "bytecode.h"
#include "scenario.h"
#include "RLMachine.hpp"
#include "RLModule.hpp"
#include "RLOperation.hpp"
#include "expression.h"

#include <sstream>
#include <iostream>

using namespace std;
using namespace LIBRL_NAMESPACE;

RLMachine::RLMachine(Archive& inArchive) 
  : m_halted(false), m_haltOnException(false), archive(inArchive)
{
  // Arbitrarily set the scenario to the first one in the archive, which is what we want until
  // we get the Gameexe.ini file parser working
  Reallive::Scenario* scenario = inArchive.scenario(archive.begin()->first);
  callStack.push(StackFrame(scenario, scenario->begin()));
}

RLMachine::~RLMachine()
{}

void RLMachine::attatchModule(RLModule* module) 
{
  int moduleType = module->moduleType();
  int moduleNumber = module->moduleNumber();
  unsigned int packedModule = packModuleNumber(moduleType, moduleNumber);

  modules.insert(packedModule, module);
}

void RLMachine::executeNextInstruction() 
{
  // Do not execute any more instructions if the machine is halted.
  if(halted() == true)
    return;

  try {

    // Refactor this out into a virtual function?
    // @todo Yeah, really refactor this ugly mess below before it grows and eats
    // tokyo
    // Switch to the proper handler based on the type of this bytecode element
    switch(callStack.top().ip->type()) {
    // Handle all the other stuff
    case Expression:
      executeExpression(static_cast<const LIBRL_NAMESPACE::ExpressionElement&>(
                          *(callStack.top().ip)));
      break;
    case Command:
    case Function:
    case Select:
    case Goto:
    case GotoCase:
    case GotoOn:
      executeCommand(static_cast<const LIBRL_NAMESPACE::CommandElement&>(
                       *(callStack.top().ip)));
      break;
    default:
      // Increment the IP for things we don't handle yet or very well.
      advanceInstructionPointer();
    }
  }
  catch(std::exception& e) {
    if(m_haltOnException) {
      m_halted = true;
      cerr << "ERROR: ";
    } else {
      cerr << "WARNING: ";
    }

    cerr << "Uncaught exception: " << e.what() << endl;
  }
}

// -----------------------------------------------------------------------

void RLMachine::executeUntilHalted()
{
  while(!halted()) {
    executeNextInstruction();
  }
}

// -----------------------------------------------------------------------

void RLMachine::advanceInstructionPointer()
{
  callStack.top().ip++;
  if(callStack.top().ip == callStack.top().scenario->end())
    m_halted = true;
}

// -----------------------------------------------------------------------

/**
 *
 * @note This method was plagarized from xclannad.
 */
int RLMachine::getIntValue(int type, int location) 
{
  int index = type % 26;
  type /= 26;
  if(index == INTZ_LOCATION_IN_BYTECODE) index = INTZ_LOCATION;
  if(index == INTL_LOCATION_IN_BYTECODE) index = INTL_LOCATION;
  if(index > NUMBER_OF_INT_LOCATIONS) return 0;
  if (type == 0) {
    // A[]..G[], Z[] ��ľ���ɤ�
    if (uint(location) >= 2000) return 0;
    return intVar[index][location];
  } else {
    // Ab[]..G4b[], Z8b[] �ʤɤ��ɤ�
    int factor = 1 << (type - 1);
    int eltsize = 32 / factor;
    if (uint(location) >= (64000 / factor)) return 0;
    return (intVar[index][location / eltsize] >>
            ((location % eltsize) * factor)) & ((1 << factor) - 1);
  }
}

/**
 *
 * @note This method was plagarized from xclannad.
 */
void RLMachine::setIntValue(int rawtype, int location, int value) {
//  cerr << "setIntValue(" << rawtype << ", " << location << ", " << value << ")" << endl;

  int type = rawtype / 26;
  int index = rawtype % 26;
  if (index == INTZ_LOCATION_IN_BYTECODE) index = INTZ_LOCATION;
  if (index == INTL_LOCATION_IN_BYTECODE) index = INTL_LOCATION;
  if (index < 0 || index > NUMBER_OF_INT_LOCATIONS) {
    fprintf(stderr, "Error: invalid access to Var<%d>[%d]\n",
            type, location);
  }
  if (type == 0) {
    // A[]..G[], Z[] ��ľ�˽�
    if (uint(location) >= 2000) return;
    intVar[index][location] = value;
  } else {
    // Ab[]..G4b[], Z8b[] �ʤɤ��
    int factor = 1 << (type - 1);
    int eltsize = 32 / factor;
    int eltmask = (1 << factor) - 1;
    int shift = (location % eltsize) * factor;
    if (uint(location) >= (64000 / factor)) return;
    intVar[index][location / eltsize] =
      (intVar[index][location / eltsize] & ~(eltmask << shift))
      | (value & eltmask) << shift;
  }
}

const std::string& RLMachine::getStringValue(int type, int location) {
  switch(type) {
  case 0x12: return strS[location];
  case 0x0C: return strM[location];
  default:
    throw Error("Invalid type in RLMachine::getStringValue");
  }
}

void RLMachine::setStringValue(int type, int number, const std::string& value) {
  switch(type) {
  case 0x12: 
    strS[number] = value;
    break;
  case 0x0C:
    strM[number] = value;
    break;
  default:
    throw Error("Invalid type in RLMachine::setStringValue");
  }     
}

void RLMachine::executeCommand(const CommandElement& f) {
  ModuleMap::iterator it = modules.find(packModuleNumber(f.modtype(), f.module()));
  if(it != modules.end()) {
    it->dispatchFunction(*this, f);
  } else {
    stringstream ss;
    ss << "Undefined module<" << f.modtype() << ":" << f.module() << ">";
    throw Error(ss.str());
  }
}

// -----------------------------------------------------------------------

void RLMachine::gotoLocation(BytecodeList::iterator newLocation) {
  // Modify the current frame of the call stack so that it's 
  callStack.top().ip = newLocation;
}

void RLMachine::executeExpression(const ExpressionElement& e) {
  int value = e.parsedExpression().getIntegerValue(*this);

  // Increment the instruction pointer.
  callStack.top().ip++;
}

unsigned int RLMachine::packModuleNumber(int modtype, int module)
{
  return (modtype << 8) | module;
}

void RLMachine::unpackModuleNumber(unsigned int packedModuleNumber, int& modtype,  int& module)
{
  modtype = packedModuleNumber >> 8;
  module = packedModuleNumber && 0xFF;
}