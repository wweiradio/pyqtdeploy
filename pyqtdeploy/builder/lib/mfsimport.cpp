// Copyright (c) 2014 Riverbank Computing Limited.
//
// This file is part of pyqtdeploy.
//
// This file may be used under the terms of the GNU General Public License
// v2 or v3 as published by the Free Software Foundation which can be found in
// the files LICENSE-GPL2.txt and LICENSE-GPL3.txt included in this package.
// In addition, as a special exception, Riverbank gives you certain additional
// rights.  These rights are described in the Riverbank GPL Exception, which
// can be found in the file GPL-Exception.txt in this package.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.


#include <Python.h>
#include <marshal.h>
#include <structmember.h>

#include <QByteArray>
#include <QChar>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVector>


extern "C" {

// The importer object structure.
typedef struct _mfsimporter
{
    PyObject_HEAD

    // The path that the importer handles.  It will be the name of a directory.
    QString *path;

    // The name of the module that implements __main__.
    PyObject *main;
} MfsImporter;


// C linkage forward declarations.
static int mfsimporter_init(PyObject *self, PyObject *args, PyObject *kwds);
static void mfsimporter_dealloc(PyObject *self);
static PyObject *mfsimporter_find_loader(PyObject *self, PyObject *args);
static PyObject *mfsimporter_load_module(PyObject *self, PyObject *args);
PyMODINIT_FUNC PyInit_mfsimport();


// The method table.
static PyMethodDef mfsimporter_methods[] = {
    {"find_loader", mfsimporter_find_loader, METH_VARARGS, NULL},
    {"load_module", mfsimporter_load_module, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};


// The members table.
static PyMemberDef mfsimporter_members[] = {
    {(char *)"main", T_OBJECT, offsetof(MfsImporter, main), 0, NULL},
    {NULL, 0, 0, 0, NULL}
};


// The importer type structure.
static PyTypeObject MfsImporter_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "mfsimport.mfsimporter",
    sizeof (MfsImporter),
    0,                                          // tp_itemsize
    mfsimporter_dealloc,                        // tp_dealloc
    0,                                          // tp_print
    0,                                          // tp_getattr
    0,                                          // tp_setattr
    0,                                          // tp_reserved
    0,                                          // tp_repr
    0,                                          // tp_as_number
    0,                                          // tp_as_sequence
    0,                                          // tp_as_mapping
    0,                                          // tp_hash
    0,                                          // tp_call
    0,                                          // tp_str
    0,                                          // tp_getattro
    0,                                          // tp_setattro
    0,                                          // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                         // tp_flags
    0,                                          // tp_doc
    0,                                          // tp_traverse
    0,                                          // tp_clear
    0,                                          // tp_richcompare
    0,                                          // tp_weaklistoffset
    0,                                          // tp_iter
    0,                                          // tp_iternext
    mfsimporter_methods,                        // tp_methods
    mfsimporter_members,                        // tp_members
    0,                                          // tp_getset
    0,                                          // tp_base
    0,                                          // tp_dict
    0,                                          // tp_descr_get
    0,                                          // tp_descr_set
    0,                                          // tp_dictoffset
    mfsimporter_init,                           // tp_init
    0,                                          // tp_alloc
    0,                                          // tp_new
    0,                                          // tp_free
    0,                                          // tp_is_gc
    0,                                          // tp_bases
    0,                                          // tp_mro
    0,                                          // tp_cache
    0,                                          // tp_subclasses
    0,                                          // tp_weaklist
    0,                                          // tp_del
    0                                           // tp_version_tag
};


// The module definition structure.
static struct PyModuleDef mfsimportmodule = {
    PyModuleDef_HEAD_INIT,
    "mfsimport",
    NULL,
    -1,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

}


// The different results that can be returned by find_module().
enum ModuleType {
    ModuleNotFound,
    ModuleIsModule,
    ModuleIsPackage,
    ModuleIsNamespace
};


// Other forward declarations.
static ModuleType find_module(MfsImporter *self, const QString &fqmn,
        QString &pathname, QString &filename);
static QString unicode_to_qstring(PyObject *unicode);
static PyObject *qstring_to_unicode(const QString &qstring);


// The importer initialisation function.
static int mfsimporter_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *path;

    // It's not clear if this is part of the public API.
    if (!_PyArg_NoKeywords("mfsimporter()", kwds))
        return -1;

    if (!PyArg_ParseTuple(args, "O&:mfsimporter", PyUnicode_FSDecoder, &path))
        return -1;

    QString *q_path = new QString(unicode_to_qstring(path));

    if (!QFileInfo(*q_path).isDir())
    {
        delete q_path;

        PyErr_SetString(PyExc_ImportError, "mfsimporter: not an Mfs file");
        return -1;
    }

    ((MfsImporter *)self)->path = q_path;

    return 0;
}


// The importer deallocation function.
static void mfsimporter_dealloc(PyObject *self)
{
    if (((MfsImporter *)self)->path)
    {
        delete ((MfsImporter *)self)->path;
        ((MfsImporter *)self)->path = 0;
    }

    Py_CLEAR(((MfsImporter *)self)->main);

    Py_TYPE(self)->tp_free(self);
}


// Implement the standard find_loader() method for the importer.
static PyObject *mfsimporter_find_loader(PyObject *self, PyObject *args)
{
    PyObject *py_fqmn;

    if (!PyArg_ParseTuple(args, "U:mfsimporter.find_loader", &py_fqmn))
        return NULL;

    QString fqmn = unicode_to_qstring(py_fqmn);
    QString pathname, filename;
    PyObject *result;

    switch (find_module((MfsImporter *)self, fqmn, pathname, filename))
    {
    case ModuleIsModule:
    case ModuleIsPackage:
        result = Py_BuildValue("O[]", self);
        break;

    case ModuleIsNamespace:
        {
            PyObject *py_pathname = qstring_to_unicode(pathname);
            if (!py_pathname)
                return NULL;

            result = Py_BuildValue("O[N]", Py_None, py_pathname);
            break;
        }

    case ModuleNotFound:
        result = Py_BuildValue("O[]", Py_None);
        break;
    }

    return result;
}


// Implement the standard load_module() method for the importer.
static PyObject *mfsimporter_load_module(PyObject *self, PyObject *args)
{
    PyObject *py_fqmn, *code, *py_filename, *mod_dict;

    if (!PyArg_ParseTuple(args, "U:mfsimporter.load_module", &py_fqmn))
        return NULL;

    QString fqmn = unicode_to_qstring(py_fqmn);
    QString pathname, filename;

    ModuleType mt = find_module((MfsImporter *)self, fqmn, pathname, filename);

    if (mt != ModuleIsModule && mt != ModuleIsPackage)
    {
        PyErr_Format(PyExc_ImportError, "mfsimporter: can't find module %R",
                py_fqmn);
        return NULL;
    }

    // Read in the code object from the file.
    QFile mfile(filename);

    if (!mfile.open(QIODevice::ReadOnly))
    {
        PyErr_Format(PyExc_ImportError,
                "mfsimporter: error opening file for module %R", py_fqmn);
        return NULL;
    }

    QByteArray data = mfile.readAll();

    mfile.close();

    code = PyMarshal_ReadObjectFromString(data.data(), data.size());
    if (!code)
        return NULL;

    // Get the module object and its dict.
    PyObject *mod = PyImport_AddModuleObject(py_fqmn);
    if (!mod)
        goto error;

    mod_dict = PyModule_GetDict(mod);

    // Set the loader object.
    if (PyDict_SetItemString(mod_dict, "__loader__", self) != 0)
        goto error;

    if (mt == ModuleIsPackage)
    {
        // Add __path__ to the module before the code gets executed.

        PyObject *py_pathname = qstring_to_unicode(pathname);
        if (!py_pathname)
            goto error;

        PyObject *path_list = Py_BuildValue("[N]", py_pathname);
        if (!path_list)
            goto error;

        int rc = PyDict_SetItemString(mod_dict, "__path__", path_list);
        Py_DECREF(path_list);

        if (rc != 0)
            goto error;
    }

    // Replace the module name with __main__ if it is the entry point.
    if (((MfsImporter *)self)->main && PyUnicode_Compare(py_fqmn, ((MfsImporter *)self)->main) == 0)
    {
        static PyObject *main = NULL;

        if (!main)
        {
            main = PyUnicode_FromString("__main__");
            if (!main)
                goto error;
        }

        py_fqmn = main;
    }

    py_filename = qstring_to_unicode(filename);
    if (!py_filename)
        goto error;

    mod = PyImport_ExecCodeModuleObject(py_fqmn, code, py_filename, NULL);

    Py_DECREF(py_filename);
    Py_DECREF(code);

    return mod;

error:
    Py_DECREF(code);
    return NULL;
}


// Find a fully qualified module name handled by an importer and return its
// type, path name and file name.
static ModuleType find_module(MfsImporter *self, const QString &fqmn,
        QString &pathname, QString &filename)
{
    pathname = *self->path + "/" + fqmn.split(QChar('.')).last();

    // See if it is an ordinary module.
    filename = pathname + ".pyf";

    if (QFileInfo(filename).isFile())
        return ModuleIsModule;

    // See if it is a package.
    filename = pathname + "/__init__.pyf";

    if (QFileInfo(filename).isFile())
        return ModuleIsPackage;

    // See if it is a namespace.
    filename = pathname;

    if (QFileInfo(filename).isDir())
        return ModuleIsNamespace;

    // Nothing was found.
    return ModuleNotFound;
}


// Convert a Python unicode object to a QString.
static QString unicode_to_qstring(PyObject *unicode)
{
    Py_ssize_t len = PyUnicode_GET_LENGTH(unicode);

    switch (PyUnicode_KIND(unicode))
    {
    case PyUnicode_1BYTE_KIND:
        return QString::fromLatin1((char *)PyUnicode_1BYTE_DATA(unicode), len);

    case PyUnicode_2BYTE_KIND:
        // The (QChar *) cast should be safe.
        return QString((QChar *)PyUnicode_2BYTE_DATA(unicode), len);

    case PyUnicode_4BYTE_KIND:
        return QString::fromUcs4(PyUnicode_4BYTE_DATA(unicode), len);
    }

    return QString();
}


// Convert a QString to a Python unicode object.
static PyObject *qstring_to_unicode(const QString &qstring)
{
    QVector<uint> ucs4 = qstring.toUcs4();

    return PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, ucs4.data(),
            ucs4.length());
}


// The module initialisation function.
PyMODINIT_FUNC PyInit_mfsimport()
{
    PyObject *mod;

    // Just in case we are linking against Python as a Windows DLL.
    MfsImporter_Type.tp_new = PyType_GenericNew;

    if (PyType_Ready(&MfsImporter_Type) < 0)
        return NULL;

    mod = PyModule_Create(&mfsimportmodule);
    if (mod == NULL)
        return NULL;

    Py_INCREF(&MfsImporter_Type);
    if (PyModule_AddObject(mod, "mfsimporter", (PyObject *)&MfsImporter_Type) < 0)
    {
        Py_DECREF(mod);
        return NULL;
    }

    return mod;
}