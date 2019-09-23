/****************************************************************************
** Meta object code from reading C++ file 'propertydelegate.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.13.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "PropertyEditor/propertydelegate.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'propertydelegate.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.13.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_PropertyDelegate_t {
    QByteArrayData data[13];
    char stringdata0[131];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_PropertyDelegate_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_PropertyDelegate_t qt_meta_stringdata_PropertyDelegate = {
    {
QT_MOC_LITERAL(0, 0, 16), // "PropertyDelegate"
QT_MOC_LITERAL(1, 17, 11), // "itemChanged"
QT_MOC_LITERAL(2, 29, 0), // ""
QT_MOC_LITERAL(3, 30, 11), // "QModelIndex"
QT_MOC_LITERAL(4, 42, 3), // "idx"
QT_MOC_LITERAL(5, 46, 21), // "editorWidgetToDisplay"
QT_MOC_LITERAL(6, 68, 8), // "QWidget*"
QT_MOC_LITERAL(7, 77, 6), // "editor"
QT_MOC_LITERAL(8, 84, 17), // "drawBranchesAgain"
QT_MOC_LITERAL(9, 102, 9), // "QPainter*"
QT_MOC_LITERAL(10, 112, 7), // "painter"
QT_MOC_LITERAL(11, 120, 4), // "rect"
QT_MOC_LITERAL(12, 125, 5) // "index"

    },
    "PropertyDelegate\0itemChanged\0\0QModelIndex\0"
    "idx\0editorWidgetToDisplay\0QWidget*\0"
    "editor\0drawBranchesAgain\0QPainter*\0"
    "painter\0rect\0index"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_PropertyDelegate[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   29,    2, 0x06 /* Public */,
       5,    2,   32,    2, 0x06 /* Public */,
       8,    3,   37,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 6,    4,    7,
    QMetaType::Void, 0x80000000 | 9, QMetaType::QRect, 0x80000000 | 3,   10,   11,   12,

       0        // eod
};

void PropertyDelegate::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PropertyDelegate *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->itemChanged((*reinterpret_cast< QModelIndex(*)>(_a[1]))); break;
        case 1: _t->editorWidgetToDisplay((*reinterpret_cast< QModelIndex(*)>(_a[1])),(*reinterpret_cast< QWidget*(*)>(_a[2]))); break;
        case 2: _t->drawBranchesAgain((*reinterpret_cast< QPainter*(*)>(_a[1])),(*reinterpret_cast< QRect(*)>(_a[2])),(*reinterpret_cast< QModelIndex(*)>(_a[3]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QWidget* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (PropertyDelegate::*)(QModelIndex ) const;
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&PropertyDelegate::itemChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (PropertyDelegate::*)(QModelIndex , QWidget * ) const;
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&PropertyDelegate::editorWidgetToDisplay)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (PropertyDelegate::*)(QPainter * , QRect , QModelIndex ) const;
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&PropertyDelegate::drawBranchesAgain)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject PropertyDelegate::staticMetaObject = { {
    &QStyledItemDelegate::staticMetaObject,
    qt_meta_stringdata_PropertyDelegate.data,
    qt_meta_data_PropertyDelegate,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *PropertyDelegate::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PropertyDelegate::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_PropertyDelegate.stringdata0))
        return static_cast<void*>(this);
    return QStyledItemDelegate::qt_metacast(_clname);
}

int PropertyDelegate::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QStyledItemDelegate::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void PropertyDelegate::itemChanged(QModelIndex _t1)const
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(const_cast< PropertyDelegate *>(this), &staticMetaObject, 0, _a);
}

// SIGNAL 1
void PropertyDelegate::editorWidgetToDisplay(QModelIndex _t1, QWidget * _t2)const
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(const_cast< PropertyDelegate *>(this), &staticMetaObject, 1, _a);
}

// SIGNAL 2
void PropertyDelegate::drawBranchesAgain(QPainter * _t1, QRect _t2, QModelIndex _t3)const
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(const_cast< PropertyDelegate *>(this), &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
