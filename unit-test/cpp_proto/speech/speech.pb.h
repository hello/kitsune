// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: speech/speech.proto

#ifndef PROTOBUF_speech_2fspeech_2eproto__INCLUDED
#define PROTOBUF_speech_2fspeech_2eproto__INCLUDED

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 3001000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 3001000 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/metadata.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/generated_enum_reflection.h>
#include <google/protobuf/unknown_field_set.h>
// @@protoc_insertion_point(includes)

// Internal implementation detail -- do not call these.
void protobuf_AddDesc_speech_2fspeech_2eproto();
void protobuf_InitDefaults_speech_2fspeech_2eproto();
void protobuf_AssignDesc_speech_2fspeech_2eproto();
void protobuf_ShutdownFile_speech_2fspeech_2eproto();

class speech_data;

enum keyword {
  NULL_ = 0,
  OK_SENSE = 1,
  STOP = 2,
  SNOOZE = 3,
  ALEXA = 4,
  OKAY = 5
};
bool keyword_IsValid(int value);
const keyword keyword_MIN = NULL_;
const keyword keyword_MAX = OKAY;
const int keyword_ARRAYSIZE = keyword_MAX + 1;

const ::google::protobuf::EnumDescriptor* keyword_descriptor();
inline const ::std::string& keyword_Name(keyword value) {
  return ::google::protobuf::internal::NameOfEnum(
    keyword_descriptor(), value);
}
inline bool keyword_Parse(
    const ::std::string& name, keyword* value) {
  return ::google::protobuf::internal::ParseNamedEnum<keyword>(
    keyword_descriptor(), name, value);
}
// ===================================================================

class speech_data : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:speech_data) */ {
 public:
  speech_data();
  virtual ~speech_data();

  speech_data(const speech_data& from);

  inline speech_data& operator=(const speech_data& from) {
    CopyFrom(from);
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _internal_metadata_.unknown_fields();
  }

  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return _internal_metadata_.mutable_unknown_fields();
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const speech_data& default_instance();

  static const speech_data* internal_default_instance();

  void Swap(speech_data* other);

  // implements Message ----------------------------------------------

  inline speech_data* New() const { return New(NULL); }

  speech_data* New(::google::protobuf::Arena* arena) const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const speech_data& from);
  void MergeFrom(const speech_data& from);
  void Clear();
  bool IsInitialized() const;

  size_t ByteSizeLong() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  ::google::protobuf::uint8* InternalSerializeWithCachedSizesToArray(
      bool deterministic, ::google::protobuf::uint8* output) const;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output) const {
    return InternalSerializeWithCachedSizesToArray(false, output);
  }
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  void InternalSwap(speech_data* other);
  void UnsafeMergeFrom(const speech_data& from);
  private:
  inline ::google::protobuf::Arena* GetArenaNoVirtual() const {
    return _internal_metadata_.arena();
  }
  inline void* MaybeArenaPtr() const {
    return _internal_metadata_.raw_arena_ptr();
  }
  public:

  ::google::protobuf::Metadata GetMetadata() const;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // optional .keyword word = 1;
  bool has_word() const;
  void clear_word();
  static const int kWordFieldNumber = 1;
  ::keyword word() const;
  void set_word(::keyword value);

  // optional int32 confidence = 2;
  bool has_confidence() const;
  void clear_confidence();
  static const int kConfidenceFieldNumber = 2;
  ::google::protobuf::int32 confidence() const;
  void set_confidence(::google::protobuf::int32 value);

  // optional int32 version = 3;
  bool has_version() const;
  void clear_version();
  static const int kVersionFieldNumber = 3;
  ::google::protobuf::int32 version() const;
  void set_version(::google::protobuf::int32 value);

  // @@protoc_insertion_point(class_scope:speech_data)
 private:
  inline void set_has_word();
  inline void clear_has_word();
  inline void set_has_confidence();
  inline void clear_has_confidence();
  inline void set_has_version();
  inline void clear_has_version();

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  ::google::protobuf::internal::HasBits<1> _has_bits_;
  mutable int _cached_size_;
  int word_;
  ::google::protobuf::int32 confidence_;
  ::google::protobuf::int32 version_;
  friend void  protobuf_InitDefaults_speech_2fspeech_2eproto_impl();
  friend void  protobuf_AddDesc_speech_2fspeech_2eproto_impl();
  friend void protobuf_AssignDesc_speech_2fspeech_2eproto();
  friend void protobuf_ShutdownFile_speech_2fspeech_2eproto();

  void InitAsDefaultInstance();
};
extern ::google::protobuf::internal::ExplicitlyConstructed<speech_data> speech_data_default_instance_;

// ===================================================================


// ===================================================================

#if !PROTOBUF_INLINE_NOT_IN_HEADERS
// speech_data

// optional .keyword word = 1;
inline bool speech_data::has_word() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void speech_data::set_has_word() {
  _has_bits_[0] |= 0x00000001u;
}
inline void speech_data::clear_has_word() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void speech_data::clear_word() {
  word_ = 0;
  clear_has_word();
}
inline ::keyword speech_data::word() const {
  // @@protoc_insertion_point(field_get:speech_data.word)
  return static_cast< ::keyword >(word_);
}
inline void speech_data::set_word(::keyword value) {
  assert(::keyword_IsValid(value));
  set_has_word();
  word_ = value;
  // @@protoc_insertion_point(field_set:speech_data.word)
}

// optional int32 confidence = 2;
inline bool speech_data::has_confidence() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void speech_data::set_has_confidence() {
  _has_bits_[0] |= 0x00000002u;
}
inline void speech_data::clear_has_confidence() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void speech_data::clear_confidence() {
  confidence_ = 0;
  clear_has_confidence();
}
inline ::google::protobuf::int32 speech_data::confidence() const {
  // @@protoc_insertion_point(field_get:speech_data.confidence)
  return confidence_;
}
inline void speech_data::set_confidence(::google::protobuf::int32 value) {
  set_has_confidence();
  confidence_ = value;
  // @@protoc_insertion_point(field_set:speech_data.confidence)
}

// optional int32 version = 3;
inline bool speech_data::has_version() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void speech_data::set_has_version() {
  _has_bits_[0] |= 0x00000004u;
}
inline void speech_data::clear_has_version() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void speech_data::clear_version() {
  version_ = 0;
  clear_has_version();
}
inline ::google::protobuf::int32 speech_data::version() const {
  // @@protoc_insertion_point(field_get:speech_data.version)
  return version_;
}
inline void speech_data::set_version(::google::protobuf::int32 value) {
  set_has_version();
  version_ = value;
  // @@protoc_insertion_point(field_set:speech_data.version)
}

inline const speech_data* speech_data::internal_default_instance() {
  return &speech_data_default_instance_.get();
}
#endif  // !PROTOBUF_INLINE_NOT_IN_HEADERS

// @@protoc_insertion_point(namespace_scope)

#ifndef SWIG
namespace google {
namespace protobuf {

template <> struct is_proto_enum< ::keyword> : ::google::protobuf::internal::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor< ::keyword>() {
  return ::keyword_descriptor();
}

}  // namespace protobuf
}  // namespace google
#endif  // SWIG

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_speech_2fspeech_2eproto__INCLUDED