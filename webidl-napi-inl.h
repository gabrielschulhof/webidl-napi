#ifndef WEBIDL_NAPI_INL_H
#define WEBIDL_NAPI_INL_H

#include "webidl-napi.h"

namespace WebIdlNapi {

namespace details {

template <typename ArrayType, typename T, bool freeze>
static inline napi_status
ArrayToJS(napi_env env, const ArrayType& ar, napi_value* result) {
  napi_status status;
  napi_escapable_handle_scope scope;
  napi_value res;

  // TODO(gabrielschulhof): Once `napi_freeze_object` becomes available in all
  // versions of N-API at least under experimental, we can start using this
  // template param.
  (void) freeze;

  STATUS_CALL(napi_open_escapable_handle_scope(env, &scope));
  STATUS_CALL(napi_create_array(env, &res));

  for (int idx = 0; idx < ar.size(); idx++) {
    napi_value member;
    T item = ar.at(idx);

    STATUS_CALL(Converter<T>::ToJS(env, item, &member));
    STATUS_CALL(napi_set_element(env, res, idx, member));
  }

  STATUS_CALL(napi_escape_handle(env, scope, res, &res));
  STATUS_CALL(napi_close_escapable_handle_scope(env, scope));

  *result = res;
  return napi_ok;
fail:
  napi_close_escapable_handle_scope(env, scope);
  return status;
}

template <typename ArrayType, typename T>
static inline napi_status
ArrayToNative(napi_env env, napi_value ar, ArrayType* result) {
  napi_status status;
  napi_handle_scope scope;
  ArrayType res{};
  uint32_t size;

  STATUS_CALL(napi_open_handle_scope(env, &scope));
  STATUS_CALL(napi_get_array_length(env, ar, &size));

  res.resize(size);

  for (int idx = 0; idx < size; idx++) {
    napi_value member;

    STATUS_CALL(napi_get_element(env, ar, idx, &member));
    STATUS_CALL(Converter<T>::ToNative(env, member, &(res.at(idx))));
  }

  STATUS_CALL(napi_close_handle_scope(env, scope));

  *result = res;
  return napi_ok;
fail:
  napi_close_handle_scope(env, scope);
  return status;
}

}  // end of namespace details

template <>
inline napi_status
Converter<uint32_t>::ToNative(napi_env env,
                              napi_value value,
                              uint32_t* result) {
  return napi_get_value_uint32(env, value, result);
}

template <>
inline napi_status
Converter<uint32_t>::ToJS(napi_env env,
                          const uint32_t& value,
                          napi_value* result) {
  return napi_create_uint32(env, value, result);
}

template <>
inline napi_status
Converter<int32_t>::ToNative(napi_env env,
                             napi_value value,
                             int32_t* result) {
  return napi_get_value_int32(env, value, result);
}

template <>
inline napi_status
Converter<int32_t>::ToJS(napi_env env,
                         const int32_t& value,
                         napi_value* result) {
  return napi_create_int32(env, value, result);
}

template <>
inline napi_status
Converter<int64_t>::ToNative(napi_env env,
                             napi_value value,
                             int64_t* result) {
  return napi_get_value_int64(env, value, result);
}

template <>
inline napi_status
Converter<int64_t>::ToJS(napi_env env,
                         const int64_t& value,
                         napi_value* result) {
  return napi_create_int64(env, value, result);
}

template <>
inline napi_status
Converter<double>::ToNative(napi_env env,
                            napi_value value,
                            double* result) {
  return napi_get_value_double(env, value, result);
}

template <>
inline napi_status
Converter<double>::ToJS(napi_env env,
                        const double& value,
                        napi_value* result) {
  return napi_create_double(env, value, result);
}

// TODO(gabrielschulhof): DOMString should be utf16, not utf8.
template <>
inline napi_status
Converter<DOMString>::ToNative(napi_env env,
                               napi_value str,
                               DOMString* result) {
  size_t size;

  STATUS_CALL(napi_get_value_string_utf8(env, str, nullptr, 0, &size));

  result->resize(size + 1);

  STATUS_CALL(napi_get_value_string_utf8(env, str,
                                         const_cast<char*>(result->c_str()),
                                         size + 1, &size));

  return napi_ok;
}

template <>
inline napi_status
Converter<DOMString>::ToJS(napi_env env,
                           const DOMString& str,
                           napi_value* result) {
  return napi_create_string_utf8(env, str.c_str(), NAPI_AUTO_LENGTH, result);
}

template <>
inline napi_status
Converter<object>::ToNative(napi_env env,
                            napi_value val,
                            object* result) {
  *result = static_cast<object>(val);
  return napi_ok;
}

template <>
inline napi_status
Converter<object>::ToJS(napi_env env, const object& val, napi_value* result) {
  *result = static_cast<napi_value>(val);
  return napi_ok;
}

template <>
inline napi_status
Converter<unsigned long>::ToNative(napi_env env,
                                   napi_value val,
                                   unsigned long* result) {
  int64_t from_js;
  napi_status status = Converter<int64_t>::ToNative(env, val, &from_js);
  if (status == napi_ok) *result = static_cast<unsigned long>(from_js);
  return status;
}

template <>
inline napi_status
Converter<unsigned long>::ToJS(napi_env env,
                               const unsigned long& val,
                                   napi_value* result) {
  int64_t to_js = static_cast<int64_t>(val);
  return Converter<int64_t>::ToJS(env, to_js, result);
}

inline napi_status IsConstructCall(napi_env env,
                                   napi_callback_info info,
                                   const char* ifname,
                                   bool* result) {
  napi_value new_target;
  bool res = true;
  STATUS_CALL(napi_get_new_target(env, info, &new_target));

  if (new_target == nullptr) {
    STATUS_CALL(napi_throw_error(env,
                                nullptr,
                                (std::string("Non-construct calls to the `") +
                                    ifname + "` constructor are not supported.")
                                    .c_str()));
    res = false;
  }

  *result = res;
  return napi_ok;
}

inline napi_status PickSignature(napi_env env,
                                 size_t argc,
                                 napi_value* argv,
                                 std::vector<webidl_sig> sigs,
                                 int* sig_idx) {
  // Advance through the signatures one argument type at a time and mark those
  // as non-candidates whose signature does not correspond to the sequence of
  // argument types found in the actual arguments.
  for (size_t idx = 0; idx < argc; idx++) {
    napi_valuetype val_type;
    STATUS_CALL(napi_typeof(env, argv[idx], &val_type));
    for (auto& sig: sigs)
      if (sig.candidate)
        if (idx >= sig.sig.size() || sig.sig[idx] != val_type)
          sig.candidate = false;
  }

  // If any signatures are left marked as candidates, return the first one. We
  // do not touch `sig_idx` if we do not find a candidate, so the caller can set
  // it to -1 to be informed after this call completes that no candidate was
  // found.
  for (size_t idx = 0; idx < sigs.size(); idx++)
    if (sigs[idx].candidate) {
      *sig_idx = idx;
      break;
    }

  return napi_ok;
}

template <typename T>
inline void Promise<T>::Resolve(const T& result) {
  if (state != kPending) return;
  resolution = result;
  state = kResolved;
  Conclude();
}

template <typename T>
inline void Promise<T>::Reject() {
  if (state != kPending) return;
  state = kRejected;
  Conclude();
}

template <typename T>
inline void Promise<T>::Conclude() {
  if (env) NAPI_CALL_RETURN_VOID(env, Conclude(env));
}

template <typename T>
napi_status Promise<T>::Conclude(napi_env candidate_env) {
  napi_status status;
  env = (env == nullptr ? candidate_env : env);

  if (env == nullptr) return napi_ok;

  if (deferred == nullptr) {
    STATUS_CALL(napi_create_promise(env, &deferred, &promise));
  }

  if (state == kResolved) {
    napi_value js_resolution;

    STATUS_CALL(Converter<T>::ToJS(env,
                                   const_cast<const T&>(resolution),
                                   &js_resolution));
    STATUS_CALL(napi_resolve_deferred(env, deferred, js_resolution));
  } else if (state == kRejected) {
    napi_value error, message;

    STATUS_CALL(napi_create_string_utf8(env,
                                        "Promise rejected",
                                        NAPI_AUTO_LENGTH,
                                        &message));
    STATUS_CALL(napi_create_error(env, nullptr, message, &error));
    STATUS_CALL(napi_reject_deferred(env, deferred, error));
  }

  return napi_ok;
}

template <typename T>
inline napi_status Promise<T>::ToJS(napi_env env,
                                    const Promise<T>& promise,
                                    napi_value* result) {
  *result = promise.promise;
  return napi_ok;
}

template <>
template <typename T>
inline napi_status Converter<Promise<T>>::ToJS(napi_env env,
                                               const Promise<T>& promise,
                                               napi_value* result) {
  return Promise<T>::ToJS(env, promise, result);
}

template <>
template <typename T>
inline napi_status Converter<Promise<T>>::ToNative(napi_env env,
                                                   napi_value val,
                                                   Promise<T>* result) {
  return Promise<T>::ToJS(env, val, result);
}

template <typename T>
inline napi_status
sequence<T>::ToJS(napi_env env, const sequence<T>& seq, napi_value* result) {
  return details::ArrayToJS<sequence<T>, T, false>(env, seq, result);
}

template <typename T>
inline napi_status
sequence<T>::ToNative(napi_env env, napi_value val, sequence<T>* result) {
  return details::ArrayToNative<sequence<T>, T>(env, val, result);
}

template <>
template <typename T>
inline napi_status
Converter<sequence<T>>::ToJS(napi_env env,
                             const sequence<T>& val,
                             napi_value* result) {
  return sequence<T>::ToJS(env, val, result);
}

template <>
template <typename T>
inline napi_status
Converter<sequence<T>>::ToNative(napi_env env,
                                 napi_value val,
                                 sequence<T>* result) {
  return sequence<T>::ToNative(env, val, result);
}

template <typename T>
inline FrozenArray<T>::FrozenArray(std::initializer_list<T> lst):
    std::vector<T>(lst) {}

template <typename T>
inline napi_status
FrozenArray<T>::ToJS(napi_env env, const FrozenArray<T>& seq, napi_value* result) {
  return details::ArrayToJS<FrozenArray<T>, T, true>(env, seq, result);
}

template <typename T>
inline napi_status
FrozenArray<T>::ToNative(napi_env env, napi_value val, FrozenArray<T>* result) {
  return details::ArrayToNative<FrozenArray<T>, T>(env, val, result);
}

template <>
template <typename T>
inline napi_status
Converter<FrozenArray<T>>::ToJS(napi_env env,
                                const FrozenArray<T>& val,
                                napi_value* result) {
  return FrozenArray<T>::ToJS(env, val, result);
}

template <>
template <typename T>
inline napi_status
Converter<FrozenArray<T>>::ToNative(napi_env env,
                                    napi_value val,
                                    FrozenArray<T>* result) {
  return FrozenArray<T>::ToNative(env, val, result);
}

// We assume that we are in control of the instance data for this add-on. Even
// so, we also assume that there may be multiple generated files bundled into
// this add-on, each of which uses `InstanceData` to manage its state. Thus,
// if no data is set, we set a new instance, and if one is already set, we
// assume it's an instance of `InstanceData` and use that.
// static
inline napi_status
InstanceData::GetCurrent(napi_env env, InstanceData** result) {
  void* data = nullptr;

  STATUS_CALL(napi_get_instance_data(env, &data));

  if (data == nullptr) {
    InstanceData* new_data = new InstanceData;

    data = static_cast<void*>(new_data);
    napi_status status =
        napi_set_instance_data(env, data, DestroyInstanceData, nullptr);
    if (status != napi_ok) {
      delete new_data;
      return status;
    }
  }

  *result = static_cast<InstanceData*>(data);
  return napi_ok;
}

inline napi_status
InstanceData::AddConstructor(napi_env env, const char* name, napi_value ctor) {
  napi_ref ctor_ref;
  STATUS_CALL(napi_create_reference(env, ctor, 1, &ctor_ref));
  ctors[name] = ctor_ref;
  return napi_ok;
}

inline void
InstanceData::SetData(void* new_data, napi_finalize fin_cb, void* new_hint) {
  data = new_data;
  cb = fin_cb;
  hint = new_hint;
}

inline void* InstanceData::GetData() {
  return data;
}

// static
inline void
InstanceData::DestroyInstanceData(napi_env env, void* data, void* hint) {
  (void) hint;
  static_cast<InstanceData*>(data)->Destroy(env);
}

inline void InstanceData::Destroy(napi_env env) {
  for (std::pair<const char*, napi_ref> ctor: ctors) {
    NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, ctor.second));
  }

  if (data != nullptr && cb != nullptr) cb(env, data, hint);
}

inline napi_ref InstanceData::GetConstructor(const char* name) {
  return ctors[name];
}

// static
template <typename T>
napi_status Wrapping<T>::Create(napi_env env,
                                napi_value js_rcv,
                                T* cc_rcv,
                                size_t same_obj_count) {
  Wrapping<T>* wrapping = new Wrapping<T>;
  wrapping->native = cc_rcv;
  if (same_obj_count > 0)
    wrapping->refs.resize(same_obj_count, nullptr);
  return napi_wrap(env, js_rcv, wrapping, Destroy, nullptr, nullptr);
}

// static
template <typename T>
napi_status Wrapping<T>::Retrieve(napi_env env,
                                  napi_value js_rcv,
                                  T** cc_rcv,
                                  int ref_idx,
                                  napi_value* ref,
                                  Wrapping<T>** get_wrapping) {
  void* data = nullptr;

  STATUS_CALL(napi_unwrap(env, js_rcv, &data));

  Wrapping<T>*wrapping = static_cast<Wrapping<T>*>(data);

  if (ref_idx >= 0 &&
      ref_idx < wrapping->refs.size() &&
      wrapping->refs[ref_idx] != nullptr) {
    napi_value ref_value = nullptr;

    STATUS_CALL(napi_get_reference_value(env,
                                         wrapping->refs[ref_idx],
                                         &ref_value));

    if (ref != nullptr) *ref = ref_value;
  }

  if (get_wrapping != nullptr) *get_wrapping = wrapping;
  *cc_rcv = wrapping->native;
  return napi_ok;
}

template <typename T>
inline napi_status
Wrapping<T>::SetRef(napi_env env, int idx, napi_value same_obj) {
  napi_ref ref;

  STATUS_CALL(napi_create_reference(env, same_obj, 1, &ref));

  refs[idx] = ref;
  return napi_ok;
}

// static
template <typename T>
void Wrapping<T>::Destroy(napi_env env, void* data, void* hint) {
  (void) hint;
  Wrapping<T>* wrapping = static_cast<Wrapping<T>*>(data);

  for (napi_ref ref: wrapping->refs)
    if (ref != nullptr)
      NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, ref));
  delete wrapping->native;
  delete wrapping;
}

template <typename T>
template <typename FieldType,
          FieldType T::*FieldName,
          napi_property_attributes attributes,
          int sameObjId,
          bool readonly>
napi_property_descriptor Wrapping<T>::InstanceAccessor(const char* utf8name) {
  napi_property_descriptor desc = napi_property_descriptor();

  desc.utf8name = utf8name;
  desc.getter = &Wrapping<T>::InstanceGetter<FieldType, FieldName, sameObjId>;
  if (!readonly)
    desc.setter = &Wrapping<T>::InstanceSetter<FieldType, FieldName>;
  desc.attributes = attributes;

  return desc;
}

template <typename T>
template <typename FieldType, FieldType T::*FieldName, int sameIdx>
napi_value Wrapping<T>::InstanceGetter(napi_env env, napi_callback_info info) {
  napi_value js_rcv = nullptr;
  napi_value result = nullptr;
  Wrapping<T>* wrapping = nullptr;
  T* cc_rcv = nullptr;

  NAPI_CALL(env,
            napi_get_cb_info(env, info, nullptr, nullptr, &js_rcv, nullptr));

  if (sameIdx >= 0) {
    NAPI_CALL(env, Wrapping<T>::Retrieve(env,
                                         js_rcv,
                                         &cc_rcv,
                                         sameIdx,
                                         &result,
                                         &wrapping));
    if (result != nullptr) return result;
  } else {
    NAPI_CALL(env, Wrapping<T>::Retrieve(env, js_rcv, &cc_rcv));
  }

  NAPI_CALL(env, Converter<FieldType>::ToJS(env, cc_rcv->*FieldName, &result));

  if (sameIdx >= 0) NAPI_CALL(env, wrapping->SetRef(env, sameIdx, result));

  return result;
}

template <typename T>
template <typename FieldType, FieldType T::*FieldName>
napi_value Wrapping<T>::InstanceSetter(napi_env env, napi_callback_info info) {
  napi_value js_rcv;
  napi_value js_new;
  size_t argc = 1;
  T* cc_rcv;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &js_new, &js_rcv, nullptr));
  NAPI_CALL(env, Wrapping<T>::Retrieve(env, js_rcv, &cc_rcv));
  NAPI_CALL(env,
            Converter<FieldType>::ToNative(env, js_new, &(cc_rcv->*FieldName)));

  return nullptr;
}

inline napi_status
GetExposureGlobals(napi_env env,
                   std::vector<const char*> globals,
                   std::vector<napi_value>* result) {
  napi_value global;
  STATUS_CALL(napi_get_global(env, &global));

  for (const char* global_prop: globals) {
    napi_value dest, dest_proto;

    STATUS_CALL(napi_get_named_property(env, global, global_prop, &dest));
    STATUS_CALL(napi_get_named_property(env, dest, "prototype", &dest_proto));
    result->push_back(dest_proto);
  }

  return napi_ok;
}

inline napi_status
ExposeInterface(napi_env env,
                size_t prop_count,
                const napi_property_descriptor* props,
                std::vector<const char*> globals) {
  std::vector<napi_value> dests;
  STATUS_CALL(GetExposureGlobals(env, globals, &dests));
  for (napi_value dest: dests) {
    STATUS_CALL(napi_define_properties(env, dest, prop_count, props));
  }

  return napi_ok;
}

template <typename T>
template <napi_property_attributes attributes, bool readonly>
inline napi_status
ExposedPartialProperty<T>::Define(napi_env env,
                                  std::vector<const char*> globals,
                                  const char* utf8name) {
  napi_property_descriptor desc = napi_property_descriptor();

  desc.utf8name = utf8name;
  desc.attributes = attributes;
  desc.getter = &ExposedPartialProperty<T>::Getter;
  if (!readonly)
    desc.setter = &ExposedPartialProperty<T>::Setter;

  std::vector<napi_value> dests;
  STATUS_CALL(GetExposureGlobals(env, globals, &dests));

  for (napi_value dest: dests) {
    ExposedPartialProperty<T>* data = new ExposedPartialProperty<T>;
    desc.data = data;
    STATUS_CALL(napi_add_finalizer(env,
                                   dest,
                                   data,
                                   &ExposedPartialProperty<T>::Destroy,
                                   nullptr,
                                   nullptr));
    STATUS_CALL(napi_define_properties(env, dest, 1, &desc));
  }
}

template <typename T>
napi_value
ExposedPartialProperty<T>::Getter(napi_env env, napi_callback_info info) {
  void* raw_data;
  NAPI_CALL(env,
            napi_get_cb_info(env, info, nullptr, nullptr, nullptr, &raw_data));
  ExposedPartialProperty<T>* data =
      static_cast<ExposedPartialProperty<T>*>(data);

  napi_value result;

  NAPI_CALL(env, Converter<T>::ToJS(env, *data, &result));

  return result;
}

template <typename T>
napi_value
ExposedPartialProperty<T>::Setter(napi_env env, napi_callback_info info) {
  void* raw_data;
  size_t argc = 1;
  napi_value new_value;
  NAPI_CALL(env,
            napi_get_cb_info(env, info, &argc, &new_value, nullptr, &raw_data));
  ExposedPartialProperty<T>* data =
      static_cast<ExposedPartialProperty<T>*>(data);

  NAPI_CALL(env, Converter<T>::ToNative(env, new_value, &(data->value)));

  return nullptr;
}

template <typename T>
void
ExposedPartialProperty<T>::Destroy(napi_env env, void* data, void* hint) {
  (void) env;
  (void) hint;
  delete static_cast<ExposedPartialProperty<T>*>(data);
}

template <typename T>
template <napi_property_attributes attributes>
inline napi_status
ExposedPartialSameObjProperty<T>::Define(napi_env env,
                                         std::vector<const char*> globals,
                                         const char* utf8name) {
  napi_property_descriptor desc = napi_property_descriptor();

  desc.utf8name = utf8name;
  desc.attributes = attributes;
  desc.getter = &ExposedPartialSameObjProperty<T>::Getter;

  std::vector<napi_value> dests;
  STATUS_CALL(GetExposureGlobals(env, globals, &dests));

  for (napi_value dest: dests) {
    ExposedPartialSameObjProperty<T>* data =
        new ExposedPartialSameObjProperty<T>;
    desc.data = data;
    STATUS_CALL(napi_add_finalizer(env,
                                   dest,
                                   data,
                                   &ExposedPartialSameObjProperty<T>::Destroy,
                                   nullptr,
                                   nullptr));
    STATUS_CALL(napi_define_properties(env, dest, 1, &desc));
  }

  return napi_ok;
}

template <typename T>
napi_value ExposedPartialSameObjProperty<T>::Getter(napi_env env,
                                                    napi_callback_info info) {
  void* raw_data;
  NAPI_CALL(env,
            napi_get_cb_info(env, info, nullptr, nullptr, nullptr, &raw_data));
  ExposedPartialSameObjProperty<T>* data =
      static_cast<ExposedPartialSameObjProperty<T>*>(raw_data);

  napi_value result;
  if (data->value == nullptr) {
    T item;
    NAPI_CALL(env, Converter<T>::ToJS(env, item, &result));
    NAPI_CALL(env, napi_create_reference(env, result, 1, &(data->value)));
  } else {
    NAPI_CALL(env, napi_get_reference_value(env, data->value, &result));
  }

  return result;
}

template <typename T>
void ExposedPartialSameObjProperty<T>::Destroy(napi_env env,
                                               void* raw_data,
                                               void* hint) {
  ExposedPartialSameObjProperty<T>* data =
      static_cast<ExposedPartialSameObjProperty<T>*>(raw_data);
  if (data->value != nullptr)
    NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, data->value));
  delete data;
}

}  // end of namespace WebIdlNapi
#endif  // WEBIDL_NAPI_INL_H
