
#include "androcles/field.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "androcles/verification.h"

void VerifyFieldNamesAreDistinct(const std::vector<Field> fields, Verification& verification) {
  std::unordered_set<std::string> names;
  for (const Field& field : fields_) {
    if (names.find(field.name) != names.end()) {
      verification.Fail() << "Duplicate field name: " << field.name;
    }
    names.insert(field.name);
  }
}

