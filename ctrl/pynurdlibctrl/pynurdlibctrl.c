/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017, 2019, 2021, 2023-2024
 * Bastian Löher
 * Hans Toshihide Törnqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include <Python.h>
#include "structmember.h"
#include <ctrl/ctrl.h>
#include <module/module.h>
#include <module/reggen/registerlist.h>

struct CtrlClient *nurdlibctrl_client_create(PyObject *args);

struct CtrlClient;

/* The object that is used for controlling nurdlib */
typedef struct {
	PyObject_HEAD
	struct CtrlClient *client;
} NurdlibCtrl;


static void
nurdlibctrl_dealloc(NurdlibCtrl *self)
{
	//printf("nurdlibctrl_dealloc\n");
	ctrl_client_free(&self->client);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
nurdlibctrl_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	NurdlibCtrl *self;
	//printf("nurdlibctrl_new\n");

	self = (NurdlibCtrl *)type->tp_alloc(type, 0);
	if (self != NULL) {
		self->client = NULL;
	}

	return (PyObject *)self;
}

static int
nurdlibctrl_init(NurdlibCtrl *self, PyObject *args, PyObject *kwds)
{
	//printf("nurdlibctrl_init\n");
	self->client = nurdlibctrl_client_create(args);
	return 0;
}

static PyObject *
nurdlibctrl_name(NurdlibCtrl *self)
{
	(void) self;
	return PyUnicode_FromString("NurdlibCtrl");
}

static PyObject *
nurdlibctrl_crate_list(NurdlibCtrl *self)
{
	struct CtrlCrateArray array;
	size_t i;

	ctrl_client_crate_array_get(self->client, &array);
	PyObject *crate_list = PyList_New(array.num);
	for (i = 0; array.num > i; ++i) {
		struct CtrlCrate const *crate;
		size_t j;

		crate = &array.array[i];
		PyObject *module_list = PyList_New(crate->module_num);
		for (j = 0; j < crate->module_num; ++j) {
			struct CtrlModule const *module;

			module = &crate->module_array[j];
			PyList_SetItem(module_list, j,
			    PyUnicode_FromString(
			    keyword_get_string(module->type)));
		}
		PyObject *crate_tuple = PyTuple_New(2);
		PyTuple_SetItem(crate_tuple, 0,
		    PyUnicode_FromString(crate->name));
		PyTuple_SetItem(crate_tuple, 1, module_list);
		PyList_SetItem(crate_list, i, crate_tuple);
	}
	ctrl_client_crate_array_free(&array);

	return crate_list;
}

static PyObject *
nurdlibctrl_crate_info(NurdlibCtrl *self, PyObject *args)
{
	int crate_index;
	struct CtrlCrateInfo info;

	if (!PyArg_ParseTuple(args, "i", &crate_index)) {
		return NULL;
	}

	ctrl_client_crate_info_get(self->client, &info, crate_index);
	PyObject *list = PyList_New(1);
	PyList_SetItem(list, 0, Py_BuildValue("(si)", "acvt", info.acvt));

	return list;
}

static PyObject *
get_scalar_list(struct CtrlConfigScalarList *a_list)
{
	struct CtrlConfigScalar *scalar;
	PyObject *scalars = PyList_New(0);

	TAILQ_FOREACH(scalar, a_list, next) {
		PyObject *value = NULL;
		switch (scalar->type) {
		case CONFIG_SCALAR_EMPTY:
			value = PyUnicode_FromString("");
			break;
		case CONFIG_SCALAR_DOUBLE:
			value = Py_BuildValue("d", scalar->value.d.d);
			break;
		case CONFIG_SCALAR_INTEGER:
			value = Py_BuildValue("i", scalar->value.i32.i);
			break;
		case CONFIG_SCALAR_KEYWORD:
			value = Py_BuildValue("s", keyword_get_string(
			    scalar->value.keyword));
			break;
		case CONFIG_SCALAR_RANGE:
			value = Py_BuildValue("(ii)", scalar->value.range.first,
			    scalar->value.range.last);
			break;
		case CONFIG_SCALAR_STRING:
			value = PyUnicode_FromString(scalar->value.str);
			break;
		}
		PyList_Append(scalars, value);
	}
	return scalars;
}

static PyObject *
get_config_list(struct CtrlConfigList *a_list)
{
	struct CtrlConfig *config;
	PyObject *list = PyList_New(0);
	TAILQ_FOREACH(config, a_list, next) {
		PyObject *tuple;
		tuple = PyTuple_New(3);
		PyTuple_SetItem(tuple, 0, PyUnicode_FromString(
		    keyword_get_string(config->name)));
		PyTuple_SetItem(tuple, 1,
		    get_scalar_list(&config->scalar_list));
		PyObject *third = NULL;
		if (CONFIG_BLOCK == config->type) {
			third = get_config_list(&config->child_list);
		} else {
			third = PyUnicode_FromString("");
		}
		PyTuple_SetItem(tuple, 2, third);
		PyList_Append(list, tuple);
	}
	return list;
}

static PyObject *
nurdlibctrl_config_dump(NurdlibCtrl *self)
{
	int ret;
	struct CtrlConfigList config_list;
	PyObject *list = NULL;

	TAILQ_INIT(&config_list);
	ret = ctrl_client_config_dump(self->client, &config_list);
	if (ret) {
		list = get_config_list(&config_list);
		ctrl_config_dump_free(&config_list);
	}

	return list;
}

static PyObject *
get_register_list(struct CtrlRegisterArray *a_register_array)
{
	struct RegisterEntryClient const *reg_list;
	size_t i;

	reg_list = module_register_list_client_get(
	    a_register_array->module_type);

	PyObject *list = PyList_New(0);

	for (i = 0; i < a_register_array->num; ++i) {
		PyObject *tuple = PyTuple_New(3);
		struct CtrlRegister const *reg;
		int j, num;

		reg = &a_register_array->reg_array[i];
		PyTuple_SetItem(tuple, 0, PyUnicode_FromString(
		    reg_list[i].name));
		PyTuple_SetItem(tuple, 1, Py_BuildValue("i",
		    reg_list[i].address));

		num = MAX(1, reg_list[i].array_length);

		PyObject *arr_list = PyList_New(0);
		for (j = 0; j < num; ++j) {
			PyList_Append(arr_list, Py_BuildValue("i",
			    reg->value[j]));
		}
		PyTuple_SetItem(tuple, 2, arr_list);

		PyList_Append(list, tuple);
	}
	return list;
}

static PyObject *
nurdlibctrl_register_dump(NurdlibCtrl *self, PyObject *args)
{
	struct CtrlRegisterArray register_array;
	PyObject *list = NULL;
	int crate_index;
	int group_index;
	int module_index;
	int ret;

	if (!PyArg_ParseTuple(args, "iii", &crate_index, &group_index,
	    &module_index)) {
		return NULL;
	}

	ret = ctrl_client_register_array_get(self->client, &register_array,
	    crate_index, group_index, module_index);
	if (ret) {
		list = get_register_list(&register_array);
		ctrl_client_register_array_free(&register_array);
	}

	return list;
}

static PyMemberDef
nurdlibctrl_members[] = {
	{"client", T_OBJECT_EX, offsetof(NurdlibCtrl, client), 0, "client"},
	{NULL}
};

static PyMethodDef
nurdlibctrl_methods[] = {
	{"name", (PyCFunction)nurdlibctrl_name, METH_NOARGS,
		"Return the name"
	},
	{"crate_list", (PyCFunction)nurdlibctrl_crate_list, METH_NOARGS,
		"Show the list of crates, groups, and modules"
	},
	{"crate_info", (PyCFunction)nurdlibctrl_crate_info, METH_VARARGS,
		"Show various fun info about specified crate"
	},
	{"config_dump", (PyCFunction)nurdlibctrl_config_dump, METH_NOARGS,
		"Dump the current config for the host"
	},
	{"register_dump", (PyCFunction)nurdlibctrl_register_dump, METH_VARARGS,
		"Dump register information of specified module"
	},
	{NULL}
};

static PyTypeObject
NurdlibCtrlType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"pynurdlibctrl.NurdlibCtrl",
	sizeof(NurdlibCtrl),
	0,                         /* tp_itemsize */
	(destructor)nurdlibctrl_dealloc, /* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_compare */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT |
	    Py_TPFLAGS_BASETYPE,   /* tp_flags */
	"Nurdlib Control object",  /* tp_doc */
	0,                         /* tp_traverse */
	0,                         /* tp_clear */
	0,                         /* tp_richcompare */
	0,                         /* tp_weaklistoffset */
	0,                         /* tp_iter */
	0,                         /* tp_iternext */
	nurdlibctrl_methods,       /* tp_methods */
	nurdlibctrl_members,       /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)nurdlibctrl_init,/* tp_init */
	0,                         /* tp_alloc */
	nurdlibctrl_new,           /* tp_new */
};

#if PY_MAJOR_VERSION >= 3
/* Further module methods */
static PyMethodDef PyNurdlibCtrlMethods[] = {
	{NULL}
};

/* The module definition */
static struct PyModuleDef pynurdlibctrlmodule = {
	PyModuleDef_HEAD_INIT,
	"pynurdlibctrl",
	"The NurdlibCtrl module",
	-1,
	PyNurdlibCtrlMethods
};
#endif

PyMODINIT_FUNC
#if PY_MAJOR_VERSION < 3
initpynurdlibctrl(void)
#else
PyInit_pynurdlibctrl(void)
#endif
{
	PyObject *m;

	if (PyType_Ready(&NurdlibCtrlType) < 0) {
		printf("ERROR PyType_Ready\n");
#if PY_MAJOR_VERSION < 3
		return;
#else
		return NULL;
#endif
	}

#if PY_MAJOR_VERSION >= 3
	m = PyModule_Create(&pynurdlibctrlmodule);
#else
	m = Py_InitModule3("pynurdlibctrl", nurdlibctrl_methods,
	    "Module that creates a NurdlibCtrl type.");
#endif

	Py_INCREF(&NurdlibCtrlType);
	PyModule_AddObject(m, "NurdlibCtrl",
	    (PyObject *)&NurdlibCtrlType);

#if PY_MAJOR_VERSION >= 3
	return m;
#endif
}

/* Client methods */

struct CtrlClient *
nurdlibctrl_client_create(PyObject *args)
{
	char *host;
	size_t host_len;
	int16_t port = CTRL_DEFAULT_PORT;
	struct CtrlClient *client;

	if (!PyArg_ParseTuple(args, "s#|i", &host, &host_len, &port)) {
		return NULL;
	}

	client = ctrl_client_create(host, port);
	if (client == NULL) {
		printf("ERROR client_create\n");
		return NULL;
	}

	printf("Connecting to host '%s:%d'\n", host, port);

	return client;
}
