#include "jsproxy.h"

#include "hiwire.h"
#include "js2python.h"
#include "python2js.h"

static PyObject*
JsBoundMethod_cnew(int this_, const char* name);

////////////////////////////////////////////////////////////
// JsProxy
//
// This is a Python object that provides ideomatic access to a Javascript
// object.

typedef struct
{
  PyObject_HEAD int js;
} JsProxy;

static void
JsProxy_dealloc(JsProxy* self)
{
  hiwire_decref(self->js);
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
JsProxy_Repr(PyObject* o)
{
  JsProxy* self = (JsProxy*)o;
  int idrepr = hiwire_to_string(self->js);
  PyObject* pyrepr = js2python(idrepr);
  return pyrepr;
}

static PyObject*
JsProxy_GetAttr(PyObject* o, PyObject* attr_name)
{
  JsProxy* self = (JsProxy*)o;

  PyObject* str = PyObject_Str(attr_name);
  if (str == NULL) {
    return NULL;
  }

  char* key = PyUnicode_AsUTF8(str);

  if (strncmp(key, "new", 4) == 0) {
    Py_DECREF(str);
    return PyObject_GenericGetAttr(o, attr_name);
  } else if (strncmp(key, "typeof", 7) == 0) {
    Py_DECREF(str);
    int idval = hiwire_typeof(self->js);
    PyObject* result = js2python(idval);
    hiwire_decref(idval);
    return result;
  }

  int idresult = hiwire_get_member_string(self->js, (int)key);
  Py_DECREF(str);

  if (hiwire_is_function(idresult)) {
    hiwire_decref(idresult);
    return JsBoundMethod_cnew(self->js, key);
  }

  PyObject* pyresult = js2python(idresult);
  hiwire_decref(idresult);
  return pyresult;
}

static int
JsProxy_SetAttr(PyObject* o, PyObject* attr_name, PyObject* pyvalue)
{
  JsProxy* self = (JsProxy*)o;

  PyObject* attr_name_py_str = PyObject_Str(attr_name);
  if (attr_name_py_str == NULL) {
    return -1;
  }
  char* key = PyUnicode_AsUTF8(attr_name_py_str);

  if (pyvalue == NULL) {
    hiwire_delete_member_string(self->js, (int)key);
  } else {
    int idvalue = python2js(pyvalue);
    hiwire_set_member_string(self->js, (int)key, idvalue);
    hiwire_decref(idvalue);
  }
  Py_DECREF(attr_name_py_str);

  return 0;
}

static PyObject*
JsProxy_Call(PyObject* o, PyObject* args, PyObject* kwargs)
{
  JsProxy* self = (JsProxy*)o;

  Py_ssize_t nargs = PyTuple_Size(args);

  int idargs = hiwire_array();

  for (Py_ssize_t i = 0; i < nargs; ++i) {
    int idarg = python2js(PyTuple_GET_ITEM(args, i));
    hiwire_push_array(idargs, idarg);
    hiwire_decref(idarg);
  }

  int idresult = hiwire_call(self->js, idargs);
  hiwire_decref(idargs);
  PyObject* pyresult = js2python(idresult);
  hiwire_decref(idresult);
  return pyresult;
}

static PyObject*
JsProxy_RichCompare(PyObject *a, PyObject *b, int op) {
  JsProxy* aproxy = (JsProxy*)a;

  if (!JsProxy_Check(b)) {
    switch (op) {
    case Py_EQ:
      Py_RETURN_FALSE;
    case Py_NE:
      Py_RETURN_TRUE;
    default:
      return Py_NotImplemented;
    }
  }

  int result;
  int ida = python2js(a);
  int idb = python2js(b);
  switch (op) {
  case Py_LT:
    result = hiwire_less_than(ida, idb);
    break;
  case Py_LE:
    result = hiwire_less_than_equal(ida, idb);
    break;
  case Py_EQ:
    result = hiwire_equal(ida, idb);
    break;
  case Py_NE:
    result = hiwire_not_equal(ida, idb);
    break;
  case Py_GT:
    result = hiwire_greater_than(ida, idb);
    break;
  case Py_GE:
    result = hiwire_greater_than_equal(ida, idb);
    break;
  }

  hiwire_decref(ida);
  hiwire_decref(idb);
  if (result) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
}

static PyObject*
JsProxy_GetIter(PyObject *o)
{
  Py_INCREF(o);
  return o;
}

static PyObject*
JsProxy_IterNext(PyObject *o)
{
  JsProxy* self = (JsProxy*)o;

  int idresult = hiwire_next(self->js);
  if (idresult == -1) {
    return NULL;
  }

  int iddone = hiwire_get_member_string(idresult, (int)"done");
  int done = hiwire_nonzero(iddone);
  hiwire_decref(iddone);
  if (done) {
    return NULL;
  }

  int idvalue = hiwire_get_member_string(idresult, (int)"value");
  PyObject* pyvalue = js2python(idvalue);
  hiwire_decref(idvalue);
  return pyvalue;
}

static PyObject*
JsProxy_New(PyObject* o, PyObject* args, PyObject* kwargs)
{
  JsProxy* self = (JsProxy*)o;

  Py_ssize_t nargs = PyTuple_Size(args);

  int idargs = hiwire_array();

  for (Py_ssize_t i = 0; i < nargs; ++i) {
    int idarg = python2js(PyTuple_GET_ITEM(args, i));
    hiwire_push_array(idargs, idarg);
    hiwire_decref(idarg);
  }

  int idresult = hiwire_new(self->js, idargs);
  hiwire_decref(idargs);
  PyObject* pyresult = js2python(idresult);
  hiwire_decref(idresult);
  return pyresult;
}

Py_ssize_t
JsProxy_length(PyObject* o)
{
  JsProxy* self = (JsProxy*)o;

  return hiwire_get_length(self->js);
}

PyObject*
JsProxy_subscript(PyObject* o, PyObject* pyidx)
{
  JsProxy* self = (JsProxy*)o;

  int ididx = python2js(pyidx);
  int idresult = hiwire_get_member_obj(self->js, ididx);
  hiwire_decref(ididx);
  PyObject* pyresult = js2python(idresult);
  hiwire_decref(idresult);
  return pyresult;
}

int
JsProxy_ass_subscript(PyObject* o, PyObject* pyidx, PyObject* pyvalue)
{
  JsProxy* self = (JsProxy*)o;
  int ididx = python2js(pyidx);
  if (pyvalue == NULL) {
    hiwire_delete_member_obj(self->js, ididx);
  } else {
    int idvalue = python2js(pyvalue);
    hiwire_set_member_obj(self->js, ididx, idvalue);
    hiwire_decref(idvalue);
  }
  hiwire_decref(ididx);
  return 0;
}

// clang-format off
static PyMappingMethods JsProxy_MappingMethods = {
  JsProxy_length,
  JsProxy_subscript,
  JsProxy_ass_subscript,
};
// clang-format on

static PyMethodDef JsProxy_Methods[] = { { "new",
                                           (PyCFunction)JsProxy_New,
                                           METH_VARARGS | METH_KEYWORDS,
                                           "Construct a new instance" },
                                         { NULL } };

static PyTypeObject JsProxyType = {
  .tp_name = "JsProxy",
  .tp_basicsize = sizeof(JsProxy),
  .tp_dealloc = (destructor)JsProxy_dealloc,
  .tp_call = JsProxy_Call,
  .tp_getattro = JsProxy_GetAttr,
  .tp_setattro = JsProxy_SetAttr,
  .tp_richcompare = JsProxy_RichCompare,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "A proxy to make a Javascript object behave like a Python object",
  .tp_methods = JsProxy_Methods,
  .tp_as_mapping = &JsProxy_MappingMethods,
  .tp_iter = JsProxy_GetIter,
  .tp_iternext = JsProxy_IterNext,
  .tp_repr = JsProxy_Repr
};

PyObject*
JsProxy_cnew(int idobj)
{
  JsProxy* self;
  self = (JsProxy*)JsProxyType.tp_alloc(&JsProxyType, 0);
  self->js = hiwire_incref(idobj);
  return (PyObject*)self;
}

////////////////////////////////////////////////////////////
// JsBoundMethod
//
// A special class for bound methods

const size_t BOUND_METHOD_NAME_SIZE = 256;

typedef struct
{
  PyObject_HEAD int this_;
  char name[BOUND_METHOD_NAME_SIZE];
} JsBoundMethod;

static void
JsBoundMethod_dealloc(JsBoundMethod* self)
{
  hiwire_decref(self->this_);
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
JsBoundMethod_Call(PyObject* o, PyObject* args, PyObject* kwargs)
{
  JsBoundMethod* self = (JsBoundMethod*)o;

  Py_ssize_t nargs = PyTuple_Size(args);

  int idargs = hiwire_array();

  for (Py_ssize_t i = 0; i < nargs; ++i) {
    int idarg = python2js(PyTuple_GET_ITEM(args, i));
    hiwire_push_array(idargs, idarg);
    hiwire_decref(idarg);
  }

  int idresult = hiwire_call_member(self->this_, (int)self->name, idargs);
  hiwire_decref(idargs);
  PyObject* pyresult = js2python(idresult);
  hiwire_decref(idresult);
  return pyresult;
}

static PyTypeObject JsBoundMethodType = {
  .tp_name = "JsBoundMethod",
  .tp_basicsize = sizeof(JsBoundMethod),
  .tp_dealloc = (destructor)JsBoundMethod_dealloc,
  .tp_call = JsBoundMethod_Call,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "A proxy to make it possible to call Javascript bound methods from "
            "Python."
};

static PyObject*
JsBoundMethod_cnew(int this_, const char* name)
{
  JsBoundMethod* self;
  self = (JsBoundMethod*)JsBoundMethodType.tp_alloc(&JsBoundMethodType, 0);
  self->this_ = hiwire_incref(this_);
  strncpy(self->name, name, BOUND_METHOD_NAME_SIZE);
  return (PyObject*)self;
}

////////////////////////////////////////////////////////////
// Public functions

int
JsProxy_Check(PyObject* x)
{
  return (PyObject_TypeCheck(x, &JsProxyType) ||
          PyObject_TypeCheck(x, &JsBoundMethodType));
}

int
JsProxy_AsJs(PyObject* x)
{
  JsProxy* js_proxy = (JsProxy*)x;
  return hiwire_incref(js_proxy->js);
}

int
JsProxy_init()
{
  return (PyType_Ready(&JsProxyType) || PyType_Ready(&JsBoundMethodType));
}
