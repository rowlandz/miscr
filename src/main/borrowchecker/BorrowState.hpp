#ifndef BORROWCHECKER_BORROWSTATE
#define BORROWCHECKER_BORROWSTATE

#include <llvm/ADT/DenseMap.h>
#include "common/LocatedError.hpp"
#include "common/Location.hpp"
#include "borrowchecker/AccessPath.hpp"

/// @brief A pair of locations that is default-constructable so it can easily
/// work with llvm::DenseMap.
class LocationPair {
public:
  Location fst;
  Location snd;
  LocationPair() {}
  LocationPair(Location fst, Location snd) : fst(fst), snd(snd) {}
  operator bool() const { return fst && snd; }
};

/// @brief Stores access paths and their statuses (unused, used, moved,
/// unmoved).
class BorrowState {

  /// @brief All _unused_ paths with their creation locations.
  llvm::DenseMap<AccessPath*, Location> unusedPaths;

  /// @brief All _used_ paths with their creation and use locations.
  llvm::DenseMap<AccessPath*, LocationPair> usedPaths;

  /// @brief All _moved_ paths with their move locations.
  llvm::DenseMap<AccessPath*, Location> movedPaths;

  /// @brief All _unmoved_ paths with their move and unmove locations.
  llvm::DenseMap<AccessPath*, LocationPair> unmovedPaths;

  llvm::SmallVector<LocatedError>& errors;

public:
  BorrowState(llvm::SmallVector<LocatedError>& errors) : errors(errors) {}

  // TODO: why do we need to explicitly declare this?
  void operator=(const BorrowState& other) {
    unusedPaths = other.unusedPaths;
    usedPaths = other.usedPaths;
    movedPaths = other.movedPaths;
    unmovedPaths = other.unmovedPaths;
    errors = other.errors;
  }

  const llvm::DenseMap<AccessPath*, Location>& getUnusedPaths() const
    { return unusedPaths; }

  const llvm::DenseMap<AccessPath*, LocationPair>& getUsedPaths() const
    { return usedPaths; }

  const llvm::DenseMap<AccessPath*, Location>& getMovedPaths() const
    { return movedPaths; }

  const llvm::DenseMap<AccessPath*, LocationPair>& getUnmovedPaths() const
    { return unmovedPaths; }

  /// @brief Introduces @p owner as a new unused access path.
  /// @param owner must not already exist in this borrow state in any capacity.
  /// @param loc location of the expression that introduces @p owner.
  void intro(AccessPath* owner, Location loc) {
    assert(unusedPaths.lookup(owner).notExists());
    assert(!usedPaths.lookup(owner));
    assert(movedPaths.lookup(owner).notExists());
    assert(!unmovedPaths.lookup(owner));
    unusedPaths[owner] = loc;
  }

  /// @brief Moves unused path @p owner to the `usedPaths` map.
  /// @param loc The source code location where @p owner is used.
  /// @return True iff the use was successful. Otherwise an error is pushed.
  bool use(AccessPath* owner, Location loc) {
    assert(owner != nullptr && "Tried to use nullptr");
    if (Location creationLoc = unusedPaths.lookup(owner)) {
      usedPaths[owner] = LocationPair(creationLoc, loc);
      unusedPaths.erase(owner);
      return true;
    }
    else if (LocationPair locs = usedPaths.lookup(owner)) {
      errors.push_back(LocatedError()
        << "Owned reference " << owner->asString()
        << " is already used here:\n" << locs.snd
        << "so it cannot be used later:\n" << loc
      );
      return false;
    }
    else {
      errors.push_back(LocatedError()
        << "Cannot use owned reference " << owner->asString()
        << " created outside this scope.\n" << loc
      );
      return false;
    }
  }

  /// @brief Performs a _move_ on @p path. Specifically, moves @p path into the
  /// `movedPaths` map.
  /// @param moveLoc Location of `e` in `move e`.
  /// @return true iff the move was successful.
  bool move(AccessPath* path, Location moveLoc) {
    if (Location creatLoc = unusedPaths.lookup(path)) {
      errors.push_back(LocatedError()
        << "Owned reference " << path->asString() << " created here:\n"
        << creatLoc << "cannot be moved in the same scope:\n" << moveLoc
      );
      return false;
    }
    if (LocationPair locs = usedPaths.lookup(path)) {
      errors.push_back(LocatedError()
        << "Owned reference " << path->asString() << " created here:\n"
        << locs.fst << "cannot be moved in the same scope.\n" << moveLoc
      );
    }
    if (Location prevMoveLoc = movedPaths.lookup(path)) {
      errors.push_back(LocatedError()
        << "Owned reference " << path->asString()
        << " was already moved here:\n" << prevMoveLoc
        << "so it cannot be moved later:\n" << moveLoc
      );
      return false;
    }
    unmovedPaths.erase(path);
    movedPaths[path] = moveLoc;
    return true;
  }

  /// @brief Replaces the value of a moved @p path (via a store expression).
  /// @param loc location of the LHS of a store expression
  /// @return True iff the replacement was successful.
  bool unmove(AccessPath* path, Location loc) {
    if (Location creatLoc = movedPaths.lookup(path)) {
      movedPaths.erase(path);
      unmovedPaths[path] = LocationPair(creatLoc, loc);
      return true;
    }
    errors.push_back(LocatedError()
      << "Owned reference " << path->asString()
      << " becomes inaccessible after store.\n" << loc
    );
    return false;
  }

  /// @brief This block and @p other must branch off the same `previous` block.
  /// Merges the changes that @p other makes with the changes `this` makes.
  /// Basically, if either block changes the status of an access path from
  /// its status in `previous`, then the other block must make the same change.
  /// @param mergeLoc the location that merges the two blocks (e.g., the
  ///        location of an `if-then-else` expression).
  void merge(const BorrowState& other, Location mergeLoc,
             const BorrowState& previous) {

    for (auto path : previous.unusedPaths) {
      LocationPair use1 = usedPaths.lookup(path.first);
      LocationPair use2 = other.usedPaths.lookup(path.first);
      if (static_cast<bool>(use1) != static_cast<bool>(use2))
        errors.push_back(LocatedError()
          << "Owned reference " << path.first->asString() << " created here:\n"
          << path.second << "is not used in both branches of this expression:\n"
          << mergeLoc
        );
    }

    for (auto path : previous.movedPaths) {
      Location move1 = movedPaths.lookup(path.first);
      Location move2 = other.movedPaths.lookup(path.first);
      if (static_cast<bool>(move1) != static_cast<bool>(move2))
        errors.push_back(LocatedError()
          << "Owned reference " << path.first->asString() << " moved here:\n"
          << path.second << "is not replaced by both branches:\n" << mergeLoc
        );
    }

    for (auto path : previous.unmovedPaths) {
      LocationPair unmove1 = unmovedPaths.lookup(path.first);
      LocationPair unmove2 = other.unmovedPaths.lookup(path.first);
      if (static_cast<bool>(unmove1) != static_cast<bool>(unmove2))
        errors.push_back(LocatedError()
          << "Owned reference " << path.first->asString() << " moved here:\n"
          << path.second.fst << "is treated inconsistently by two branches:\n"
          << mergeLoc
        );
    }
  }

};

#endif