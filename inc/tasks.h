//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// defines bots tasks
namespace bot {

enum class TaskId : int32_t {
  Normal = 0,
  Pause,
  MoveTo,
  FollowUser,
  PickupItem,
  Camp,
  PlantBomb,
  DefuseBomb,
  Attack,
  Hunt,
  SeekCover,
  ThrowExplosive,
  ThrowFlashbang,
  ThrowSmoke,
  DoubleJump,
  EscapeFromBomb,
  ShootBreakable,
  Hide,
  Blind,
  Spraypaint,
  Num,
  None = Num
};
YSTL_ENABLE_ENUM_HASH (TaskId);

// some hard-coded desire defines used to override calculated ones
namespace TaskPri {
constexpr auto kNormal { 35.0f };
constexpr auto kPause { 36.0f };
constexpr auto kCamp { 37.0f };
constexpr auto kSpraypaint { 38.0f };
constexpr auto kFollowUser { 39.0f };
constexpr auto kMoveTo { 50.0f };
constexpr auto kDefuseBomb { 88.0f };
constexpr auto kPlantBomb { 89.0f };
constexpr auto kAttack { 90.0f };
constexpr auto kSeekCover { 91.0f };
constexpr auto kHide { 92.0f };
constexpr auto kThrow { 99.0f };
constexpr auto kDoubleJump { 99.0f };
constexpr auto kBlind { 100.0f };
constexpr auto kShootBreakable { 100.0f };
constexpr auto kEscapeFromBomb { 100.0f };
}

// tasks definition
struct Task {
  using Function = void (Bot::*) ();

public:
  Function func {}; // corresponding exec function in bot class
  TaskId id {}; // major task/action carried out
  float desire {}; // desire (filled in) for this task
  int data {}; // additional data (node index)
  float time {}; // time task expires
  bool resume {}; // if task can be continued if interrupted

public:
  Task (Function func, TaskId id, float desire, int data, float time, bool resume) :
    func (func), id (id), desire (desire), data (data), time (time), resume (resume) {}
};

// lifo stack of bot tasks with priority promotion
template <template <typename> typename Backend = ystl::Deque> class TaskStack final : public ystl::NonCopyable {
public:
  using ValueType = Task;
  using Container = Backend<Task>;

  using iterator = typename Container::iterator;
  using const_iterator = typename Container::const_iterator;

private:
  Container tasks_ {};

private:
  template <typename C, typename... Args> static void Append (C &container, Args &&...args) {
    if constexpr (requires { container.emplace_last (ystl::forward<Args> (args)...); }) {
      container.emplace_last (ystl::forward<Args> (args)...);
    }
    else {
      container.emplace (ystl::forward<Args> (args)...);
    }
  }

  template <typename C> static void DropBack (C &container) {
    if constexpr (requires { container.discard_last (); }) {
      container.discard_last ();
    }
    else {
      container.pop ();
    }
  }

public:
  [[nodiscard]] bool Empty () const noexcept {
    return tasks_.empty ();
  }

  [[nodiscard]] size_t Length () const noexcept {
    return tasks_.size ();
  }

  [[nodiscard]] Task &Current () noexcept {
    return tasks_.last ();
  }

  [[nodiscard]] const Task &Current () const noexcept {
    return tasks_.last ();
  }

  void Reserve (size_t amount) {
    tasks_.reserve (amount);
  }

  template <typename... Args> void Emplace (Args &&...args) {
    Append (tasks_, ystl::forward<Args> (args)...);
  }

  void Pop () {
    DropBack (tasks_);
  }

  void Clear () noexcept {
    tasks_.clear ();
  }

  [[nodiscard]] Task *Find (TaskId id) {
    for (auto &task : tasks_) {
      if (task.id == id) {
        return &task;
      }
    }
    return nullptr;
  }

  bool Remove (const Task &task) {
    Container kept {};

    bool removed = false;
    for (auto &entry : tasks_) {
      if (!removed && &entry == &task) {
        removed = true;
        continue;
      }
      Append (kept, entry);
    }

    if (removed) {
      tasks_ = ystl::move (kept);
    }
    return removed;
  }

  bool Promote () {
    const size_t count = tasks_.size ();
    if (count <= 1) {
      return false;
    }

    const size_t old_top = count - 1;
    size_t hot = 0;
    size_t index = 0;
    float best = 0.0f;

    for (const auto &task : tasks_) {
      if (index == 0 || task.desire > best) {
        best = task.desire;
        hot = index;
      }
      ++index;
    }

    if (hot == old_top) {
      return false;
    }

    Container kept {};
    index = 0;

    for (const auto &task : tasks_) {
      if (index == hot || task.resume) {
        Append (kept, task);
      }
      ++index;
    }
    tasks_ = ystl::move (kept);
    return true;
  }

  // drop the finished top task and every non-resumable task it reveals
  void Complete () {
    if (tasks_.empty ()) {
      return;
    }
    do {
      DropBack (tasks_);
    } while (!tasks_.empty () && !tasks_.last ().resume);
  }

public:
  iterator begin () noexcept {
    return tasks_.begin ();
  }

  iterator end () noexcept {
    return tasks_.end ();
  }

  const_iterator begin () const noexcept {
    return tasks_.begin ();
  }

  const_iterator end () const noexcept {
    return tasks_.end ();
  }
};

// bot task management data mixin
class TasksData {
  friend struct TestHook;

public:
  using Tasks = TaskStack<>;

public:
  Tasks tasks_ {};
};

} // namespace bot
