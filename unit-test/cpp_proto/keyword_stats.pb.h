// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: keyword_stats.proto

#ifndef PROTOBUF_keyword_5fstats_2eproto__INCLUDED
#define PROTOBUF_keyword_5fstats_2eproto__INCLUDED

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
#include <google/protobuf/unknown_field_set.h>
#include "speech/speech.pb.h"
// @@protoc_insertion_point(includes)

// Internal implementation detail -- do not call these.
void protobuf_AddDesc_keyword_5fstats_2eproto();
void protobuf_InitDefaults_keyword_5fstats_2eproto();
void protobuf_AssignDesc_keyword_5fstats_2eproto();
void protobuf_ShutdownFile_keyword_5fstats_2eproto();

class IndividualKeywordHistogram;
class KeywordActivation;
class KeywordStats;

// ===================================================================

class IndividualKeywordHistogram : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:IndividualKeywordHistogram) */ {
 public:
  IndividualKeywordHistogram();
  virtual ~IndividualKeywordHistogram();

  IndividualKeywordHistogram(const IndividualKeywordHistogram& from);

  inline IndividualKeywordHistogram& operator=(const IndividualKeywordHistogram& from) {
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
  static const IndividualKeywordHistogram& default_instance();

  static const IndividualKeywordHistogram* internal_default_instance();

  void Swap(IndividualKeywordHistogram* other);

  // implements Message ----------------------------------------------

  inline IndividualKeywordHistogram* New() const { return New(NULL); }

  IndividualKeywordHistogram* New(::google::protobuf::Arena* arena) const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const IndividualKeywordHistogram& from);
  void MergeFrom(const IndividualKeywordHistogram& from);
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
  void InternalSwap(IndividualKeywordHistogram* other);
  void UnsafeMergeFrom(const IndividualKeywordHistogram& from);
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

  // repeated int32 histogram_counts = 1;
  int histogram_counts_size() const;
  void clear_histogram_counts();
  static const int kHistogramCountsFieldNumber = 1;
  ::google::protobuf::int32 histogram_counts(int index) const;
  void set_histogram_counts(int index, ::google::protobuf::int32 value);
  void add_histogram_counts(::google::protobuf::int32 value);
  const ::google::protobuf::RepeatedField< ::google::protobuf::int32 >&
      histogram_counts() const;
  ::google::protobuf::RepeatedField< ::google::protobuf::int32 >*
      mutable_histogram_counts();

  // optional .keyword key_word = 2;
  bool has_key_word() const;
  void clear_key_word();
  static const int kKeyWordFieldNumber = 2;
  ::keyword key_word() const;
  void set_key_word(::keyword value);

  // @@protoc_insertion_point(class_scope:IndividualKeywordHistogram)
 private:
  inline void set_has_key_word();
  inline void clear_has_key_word();

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  ::google::protobuf::internal::HasBits<1> _has_bits_;
  mutable int _cached_size_;
  ::google::protobuf::RepeatedField< ::google::protobuf::int32 > histogram_counts_;
  int key_word_;
  friend void  protobuf_InitDefaults_keyword_5fstats_2eproto_impl();
  friend void  protobuf_AddDesc_keyword_5fstats_2eproto_impl();
  friend void protobuf_AssignDesc_keyword_5fstats_2eproto();
  friend void protobuf_ShutdownFile_keyword_5fstats_2eproto();

  void InitAsDefaultInstance();
};
extern ::google::protobuf::internal::ExplicitlyConstructed<IndividualKeywordHistogram> IndividualKeywordHistogram_default_instance_;

// -------------------------------------------------------------------

class KeywordActivation : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:KeywordActivation) */ {
 public:
  KeywordActivation();
  virtual ~KeywordActivation();

  KeywordActivation(const KeywordActivation& from);

  inline KeywordActivation& operator=(const KeywordActivation& from) {
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
  static const KeywordActivation& default_instance();

  static const KeywordActivation* internal_default_instance();

  void Swap(KeywordActivation* other);

  // implements Message ----------------------------------------------

  inline KeywordActivation* New() const { return New(NULL); }

  KeywordActivation* New(::google::protobuf::Arena* arena) const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const KeywordActivation& from);
  void MergeFrom(const KeywordActivation& from);
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
  void InternalSwap(KeywordActivation* other);
  void UnsafeMergeFrom(const KeywordActivation& from);
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

  // optional int64 time_counter = 1;
  bool has_time_counter() const;
  void clear_time_counter();
  static const int kTimeCounterFieldNumber = 1;
  ::google::protobuf::int64 time_counter() const;
  void set_time_counter(::google::protobuf::int64 value);

  // optional .keyword key_word = 2;
  bool has_key_word() const;
  void clear_key_word();
  static const int kKeyWordFieldNumber = 2;
  ::keyword key_word() const;
  void set_key_word(::keyword value);

  // @@protoc_insertion_point(class_scope:KeywordActivation)
 private:
  inline void set_has_time_counter();
  inline void clear_has_time_counter();
  inline void set_has_key_word();
  inline void clear_has_key_word();

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  ::google::protobuf::internal::HasBits<1> _has_bits_;
  mutable int _cached_size_;
  ::google::protobuf::int64 time_counter_;
  int key_word_;
  friend void  protobuf_InitDefaults_keyword_5fstats_2eproto_impl();
  friend void  protobuf_AddDesc_keyword_5fstats_2eproto_impl();
  friend void protobuf_AssignDesc_keyword_5fstats_2eproto();
  friend void protobuf_ShutdownFile_keyword_5fstats_2eproto();

  void InitAsDefaultInstance();
};
extern ::google::protobuf::internal::ExplicitlyConstructed<KeywordActivation> KeywordActivation_default_instance_;

// -------------------------------------------------------------------

class KeywordStats : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:KeywordStats) */ {
 public:
  KeywordStats();
  virtual ~KeywordStats();

  KeywordStats(const KeywordStats& from);

  inline KeywordStats& operator=(const KeywordStats& from) {
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
  static const KeywordStats& default_instance();

  static const KeywordStats* internal_default_instance();

  void Swap(KeywordStats* other);

  // implements Message ----------------------------------------------

  inline KeywordStats* New() const { return New(NULL); }

  KeywordStats* New(::google::protobuf::Arena* arena) const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const KeywordStats& from);
  void MergeFrom(const KeywordStats& from);
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
  void InternalSwap(KeywordStats* other);
  void UnsafeMergeFrom(const KeywordStats& from);
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

  // optional string net_model = 1;
  bool has_net_model() const;
  void clear_net_model();
  static const int kNetModelFieldNumber = 1;
  const ::std::string& net_model() const;
  void set_net_model(const ::std::string& value);
  void set_net_model(const char* value);
  void set_net_model(const char* value, size_t size);
  ::std::string* mutable_net_model();
  ::std::string* release_net_model();
  void set_allocated_net_model(::std::string* net_model);

  // repeated .IndividualKeywordHistogram histograms = 2;
  int histograms_size() const;
  void clear_histograms();
  static const int kHistogramsFieldNumber = 2;
  const ::IndividualKeywordHistogram& histograms(int index) const;
  ::IndividualKeywordHistogram* mutable_histograms(int index);
  ::IndividualKeywordHistogram* add_histograms();
  ::google::protobuf::RepeatedPtrField< ::IndividualKeywordHistogram >*
      mutable_histograms();
  const ::google::protobuf::RepeatedPtrField< ::IndividualKeywordHistogram >&
      histograms() const;

  // repeated .KeywordActivation keyword_activations = 3;
  int keyword_activations_size() const;
  void clear_keyword_activations();
  static const int kKeywordActivationsFieldNumber = 3;
  const ::KeywordActivation& keyword_activations(int index) const;
  ::KeywordActivation* mutable_keyword_activations(int index);
  ::KeywordActivation* add_keyword_activations();
  ::google::protobuf::RepeatedPtrField< ::KeywordActivation >*
      mutable_keyword_activations();
  const ::google::protobuf::RepeatedPtrField< ::KeywordActivation >&
      keyword_activations() const;

  // @@protoc_insertion_point(class_scope:KeywordStats)
 private:
  inline void set_has_net_model();
  inline void clear_has_net_model();

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  ::google::protobuf::internal::HasBits<1> _has_bits_;
  mutable int _cached_size_;
  ::google::protobuf::RepeatedPtrField< ::IndividualKeywordHistogram > histograms_;
  ::google::protobuf::RepeatedPtrField< ::KeywordActivation > keyword_activations_;
  ::google::protobuf::internal::ArenaStringPtr net_model_;
  friend void  protobuf_InitDefaults_keyword_5fstats_2eproto_impl();
  friend void  protobuf_AddDesc_keyword_5fstats_2eproto_impl();
  friend void protobuf_AssignDesc_keyword_5fstats_2eproto();
  friend void protobuf_ShutdownFile_keyword_5fstats_2eproto();

  void InitAsDefaultInstance();
};
extern ::google::protobuf::internal::ExplicitlyConstructed<KeywordStats> KeywordStats_default_instance_;

// ===================================================================


// ===================================================================

#if !PROTOBUF_INLINE_NOT_IN_HEADERS
// IndividualKeywordHistogram

// repeated int32 histogram_counts = 1;
inline int IndividualKeywordHistogram::histogram_counts_size() const {
  return histogram_counts_.size();
}
inline void IndividualKeywordHistogram::clear_histogram_counts() {
  histogram_counts_.Clear();
}
inline ::google::protobuf::int32 IndividualKeywordHistogram::histogram_counts(int index) const {
  // @@protoc_insertion_point(field_get:IndividualKeywordHistogram.histogram_counts)
  return histogram_counts_.Get(index);
}
inline void IndividualKeywordHistogram::set_histogram_counts(int index, ::google::protobuf::int32 value) {
  histogram_counts_.Set(index, value);
  // @@protoc_insertion_point(field_set:IndividualKeywordHistogram.histogram_counts)
}
inline void IndividualKeywordHistogram::add_histogram_counts(::google::protobuf::int32 value) {
  histogram_counts_.Add(value);
  // @@protoc_insertion_point(field_add:IndividualKeywordHistogram.histogram_counts)
}
inline const ::google::protobuf::RepeatedField< ::google::protobuf::int32 >&
IndividualKeywordHistogram::histogram_counts() const {
  // @@protoc_insertion_point(field_list:IndividualKeywordHistogram.histogram_counts)
  return histogram_counts_;
}
inline ::google::protobuf::RepeatedField< ::google::protobuf::int32 >*
IndividualKeywordHistogram::mutable_histogram_counts() {
  // @@protoc_insertion_point(field_mutable_list:IndividualKeywordHistogram.histogram_counts)
  return &histogram_counts_;
}

// optional .keyword key_word = 2;
inline bool IndividualKeywordHistogram::has_key_word() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void IndividualKeywordHistogram::set_has_key_word() {
  _has_bits_[0] |= 0x00000002u;
}
inline void IndividualKeywordHistogram::clear_has_key_word() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void IndividualKeywordHistogram::clear_key_word() {
  key_word_ = 0;
  clear_has_key_word();
}
inline ::keyword IndividualKeywordHistogram::key_word() const {
  // @@protoc_insertion_point(field_get:IndividualKeywordHistogram.key_word)
  return static_cast< ::keyword >(key_word_);
}
inline void IndividualKeywordHistogram::set_key_word(::keyword value) {
  assert(::keyword_IsValid(value));
  set_has_key_word();
  key_word_ = value;
  // @@protoc_insertion_point(field_set:IndividualKeywordHistogram.key_word)
}

inline const IndividualKeywordHistogram* IndividualKeywordHistogram::internal_default_instance() {
  return &IndividualKeywordHistogram_default_instance_.get();
}
// -------------------------------------------------------------------

// KeywordActivation

// optional int64 time_counter = 1;
inline bool KeywordActivation::has_time_counter() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void KeywordActivation::set_has_time_counter() {
  _has_bits_[0] |= 0x00000001u;
}
inline void KeywordActivation::clear_has_time_counter() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void KeywordActivation::clear_time_counter() {
  time_counter_ = GOOGLE_LONGLONG(0);
  clear_has_time_counter();
}
inline ::google::protobuf::int64 KeywordActivation::time_counter() const {
  // @@protoc_insertion_point(field_get:KeywordActivation.time_counter)
  return time_counter_;
}
inline void KeywordActivation::set_time_counter(::google::protobuf::int64 value) {
  set_has_time_counter();
  time_counter_ = value;
  // @@protoc_insertion_point(field_set:KeywordActivation.time_counter)
}

// optional .keyword key_word = 2;
inline bool KeywordActivation::has_key_word() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void KeywordActivation::set_has_key_word() {
  _has_bits_[0] |= 0x00000002u;
}
inline void KeywordActivation::clear_has_key_word() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void KeywordActivation::clear_key_word() {
  key_word_ = 0;
  clear_has_key_word();
}
inline ::keyword KeywordActivation::key_word() const {
  // @@protoc_insertion_point(field_get:KeywordActivation.key_word)
  return static_cast< ::keyword >(key_word_);
}
inline void KeywordActivation::set_key_word(::keyword value) {
  assert(::keyword_IsValid(value));
  set_has_key_word();
  key_word_ = value;
  // @@protoc_insertion_point(field_set:KeywordActivation.key_word)
}

inline const KeywordActivation* KeywordActivation::internal_default_instance() {
  return &KeywordActivation_default_instance_.get();
}
// -------------------------------------------------------------------

// KeywordStats

// optional string net_model = 1;
inline bool KeywordStats::has_net_model() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void KeywordStats::set_has_net_model() {
  _has_bits_[0] |= 0x00000001u;
}
inline void KeywordStats::clear_has_net_model() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void KeywordStats::clear_net_model() {
  net_model_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_net_model();
}
inline const ::std::string& KeywordStats::net_model() const {
  // @@protoc_insertion_point(field_get:KeywordStats.net_model)
  return net_model_.GetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void KeywordStats::set_net_model(const ::std::string& value) {
  set_has_net_model();
  net_model_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:KeywordStats.net_model)
}
inline void KeywordStats::set_net_model(const char* value) {
  set_has_net_model();
  net_model_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:KeywordStats.net_model)
}
inline void KeywordStats::set_net_model(const char* value, size_t size) {
  set_has_net_model();
  net_model_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:KeywordStats.net_model)
}
inline ::std::string* KeywordStats::mutable_net_model() {
  set_has_net_model();
  // @@protoc_insertion_point(field_mutable:KeywordStats.net_model)
  return net_model_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* KeywordStats::release_net_model() {
  // @@protoc_insertion_point(field_release:KeywordStats.net_model)
  clear_has_net_model();
  return net_model_.ReleaseNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void KeywordStats::set_allocated_net_model(::std::string* net_model) {
  if (net_model != NULL) {
    set_has_net_model();
  } else {
    clear_has_net_model();
  }
  net_model_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), net_model);
  // @@protoc_insertion_point(field_set_allocated:KeywordStats.net_model)
}

// repeated .IndividualKeywordHistogram histograms = 2;
inline int KeywordStats::histograms_size() const {
  return histograms_.size();
}
inline void KeywordStats::clear_histograms() {
  histograms_.Clear();
}
inline const ::IndividualKeywordHistogram& KeywordStats::histograms(int index) const {
  // @@protoc_insertion_point(field_get:KeywordStats.histograms)
  return histograms_.Get(index);
}
inline ::IndividualKeywordHistogram* KeywordStats::mutable_histograms(int index) {
  // @@protoc_insertion_point(field_mutable:KeywordStats.histograms)
  return histograms_.Mutable(index);
}
inline ::IndividualKeywordHistogram* KeywordStats::add_histograms() {
  // @@protoc_insertion_point(field_add:KeywordStats.histograms)
  return histograms_.Add();
}
inline ::google::protobuf::RepeatedPtrField< ::IndividualKeywordHistogram >*
KeywordStats::mutable_histograms() {
  // @@protoc_insertion_point(field_mutable_list:KeywordStats.histograms)
  return &histograms_;
}
inline const ::google::protobuf::RepeatedPtrField< ::IndividualKeywordHistogram >&
KeywordStats::histograms() const {
  // @@protoc_insertion_point(field_list:KeywordStats.histograms)
  return histograms_;
}

// repeated .KeywordActivation keyword_activations = 3;
inline int KeywordStats::keyword_activations_size() const {
  return keyword_activations_.size();
}
inline void KeywordStats::clear_keyword_activations() {
  keyword_activations_.Clear();
}
inline const ::KeywordActivation& KeywordStats::keyword_activations(int index) const {
  // @@protoc_insertion_point(field_get:KeywordStats.keyword_activations)
  return keyword_activations_.Get(index);
}
inline ::KeywordActivation* KeywordStats::mutable_keyword_activations(int index) {
  // @@protoc_insertion_point(field_mutable:KeywordStats.keyword_activations)
  return keyword_activations_.Mutable(index);
}
inline ::KeywordActivation* KeywordStats::add_keyword_activations() {
  // @@protoc_insertion_point(field_add:KeywordStats.keyword_activations)
  return keyword_activations_.Add();
}
inline ::google::protobuf::RepeatedPtrField< ::KeywordActivation >*
KeywordStats::mutable_keyword_activations() {
  // @@protoc_insertion_point(field_mutable_list:KeywordStats.keyword_activations)
  return &keyword_activations_;
}
inline const ::google::protobuf::RepeatedPtrField< ::KeywordActivation >&
KeywordStats::keyword_activations() const {
  // @@protoc_insertion_point(field_list:KeywordStats.keyword_activations)
  return keyword_activations_;
}

inline const KeywordStats* KeywordStats::internal_default_instance() {
  return &KeywordStats_default_instance_.get();
}
#endif  // !PROTOBUF_INLINE_NOT_IN_HEADERS
// -------------------------------------------------------------------

// -------------------------------------------------------------------


// @@protoc_insertion_point(namespace_scope)

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_keyword_5fstats_2eproto__INCLUDED
