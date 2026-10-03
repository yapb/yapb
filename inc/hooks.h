//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// simple handler for parsing and rewriting queries (fake queries)
namespace bot {

class QueryBuffer {
  ystl::SmallArray<uint8_t> buffer_ {};
  size_t cursor_ {};

public:
  QueryBuffer (const uint8_t *msg, size_t length, size_t shift) : cursor_ (0) {
    buffer_.insert (0, msg, length);
    cursor_ += shift;
  }

public:
  template <typename T> T Read () {
    T result {};
    constexpr auto size = sizeof (T);

    if (cursor_ + size > buffer_.size ()) {
      return 0;
    }

    memcpy (&result, buffer_.data () + cursor_, size);
    cursor_ += size;

    return result;
  }

  // must be called right after read
  template <typename T> void Write (T value) {
    constexpr auto size = sizeof (value);
    memcpy (buffer_.data () + cursor_ - size, &value, size);
  }

  template <typename T> void Skip () {
    constexpr auto size = sizeof (T);

    if (cursor_ + size > buffer_.size ()) {
      return;
    }
    cursor_ += size;
  }

  void SkipString () {
    if (buffer_.size () < cursor_) {
      return;
    }
    for (; cursor_ < buffer_.size () && buffer_[cursor_] != ystl::kNullChar; ++cursor_) {
    }
    ++cursor_;
  }

  ystl::String ReadString () {
    if (buffer_.size () < cursor_) {
      return "";
    }
    ystl::String out;

    for (; cursor_ < buffer_.size () && buffer_[cursor_] != ystl::kNullChar; ++cursor_) {
      out += buffer_[cursor_];
    }
    ++cursor_;

    return out;
  }

  void ShiftToEnd () {
    cursor_ = buffer_.size ();
  }

public:
  ystl::Twin<const uint8_t *, size_t> Data () {
    return { buffer_.data (), buffer_.size () };
  }
};

// used for response with fake timestamps and bots count in server responses
class ServerQueryHook : public ystl::Singleton<ServerQueryHook> {
private:
  using SendToProto = decltype (sendto);

private:
  ystl::Detour<SendToProto> send_to_detour_ {}, send_to_detour_sys_ {};

public:
  ServerQueryHook () = default;
  ~ServerQueryHook () = default;

public:
  // initialzie and install hook
  void Init ();

public:
  // disables send hook
  bool Disable () {
    send_to_detour_sys_.restore ();
    return send_to_detour_.restore ();
  }

public:
  YSTL_FORCE_STACK_ALIGN static int32_t YSTL_STDCALL SendTo (
    int socket, const void *message, size_t length, int flags, const struct sockaddr *dest, int dest_length);
};

// used for transit calls between game dll and engine without all needed functions on bot side
class EntityLinkHook : public ystl::Singleton<EntityLinkHook> {
private:
  bool paused_ { false };

  ystl::Detour<ystl::PlatformDynlink::DlsymType> dlsym_ {};
  ystl::Detour<ystl::PlatformDynlink::DlcloseType> dlclose_ {};

  ystl::HashMap<ystl::StringRef, ystl::SharedLibrary::Func> exports_ {};

  ystl::SharedLibrary self_ {};

public:
  EntityLinkHook () = default;
  ~EntityLinkHook () = default;

public:
  void Initialize ();
  bool NeedsBypass () const;

  ystl::SharedLibrary::Func LookupSymbol (ystl::SharedLibrary::Handle module, const char *function);

  decltype (auto) FreeLibrary (ystl::SharedLibrary::Handle module) {
    if (self_.handle () == module) {
      Disable ();
      return dlclose_ (module);
    }
    return dlclose_ (module);
  }

public:
  bool CallPlayerFunction (edict_t *ent);

public:
  void Enable () {
    if (dlsym_.detoured ()) {
      return;
    }
    dlsym_.detour ();
  }

  void Disable () {
    if (!dlsym_.detoured ()) {
      return;
    }
    dlsym_.restore ();
  }

  void SetPaused (bool what) {
    paused_ = what;
  }

  bool IsPaused () const {
    return paused_;
  }

public:
  YSTL_FORCE_STACK_ALIGN static ystl::SharedLibrary::Func YSTL_STDCALL LookupHandler (ystl::SharedLibrary::Handle handle, const char *function) {
    return instance ().LookupSymbol (handle, function);
  }

  YSTL_FORCE_STACK_ALIGN static int YSTL_STDCALL CloseHandler (ystl::SharedLibrary::Handle handle) {
    return instance ().FreeLibrary (handle);
  }
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (EntityLinkHook, entlink);
YSTL_EXPOSE_GLOBAL_SINGLETON (ServerQueryHook, fakequeries);

} // namespace bot
