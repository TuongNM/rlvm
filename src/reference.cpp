
#include "reference.hpp"
#include "RLMachine.hpp"

/** 
 * Read from the memory location, and return the value.
 * 
 * @return The integer value of the memory location.
 */
IntAccessor::operator int() const
{
  return it->machine->getIntValue(it->type, it->location); 
}

// -----------------------------------------------------------------------

/** 
 * Assign a new value to the memory location.
 * 
 * @param newValue New value to set.
 * @return Self
 */
IntAccessor& IntAccessor::operator=(const int newValue) { 
  it->machine->setIntValue(it->type, it->location, newValue);
  return *this;
} 

// -----------------------------------------------------------------------

/** 
 * Assigns to this accessor from another IntAccessor. This allows us
 * to use the "*dest = *src" mechanic since normally, that would call
 * the default copy operator, which would copy rhs.it onto it.
 * 
 * @param rhs IntAccessor to read from
 * @return Self
 */
IntAccessor& IntAccessor::operator=(const IntAccessor& rhs)
{
  return operator=(rhs.operator int());
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

/** 
 * Read from the memory location, and return the value.
 * 
 * @return The string value of the memory location.
 */
StringAccessor::operator std::string() const
{
  return it->machine->getStringValue(it->type, it->location);
}

// -----------------------------------------------------------------------

/** 
 * Assign a new value to the memory location.
 * 
 * @param newValue New value to set.
 * @return Self
 */
StringAccessor& StringAccessor::operator=(const std::string& newValue) {
  it->machine->setStringValue(it->type, it->location, newValue);
  return *this;
}

// -----------------------------------------------------------------------

bool StringAccessor::operator==(const std::string& rhs) {
  return operator std::string() == rhs;
}

// -----------------------------------------------------------------------

/** 
 * Assigns to this accessor from another StringAccessor. This allows
 * us to use the "*dest = *src" mechanic since normally, that would
 * call the default copy operator, which would copy rhs.it onto it.
 * 
 * @param rhs StringAccessor to read from
 * @return Self
 */
StringAccessor& StringAccessor::operator=(const StringAccessor& rhs)
{
  return operator=(rhs.operator std::string());
}
