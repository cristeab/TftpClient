#ifndef QML_HELPERS_H
#define QML_HELPERS_H

#include <QObject>

#define QML_WRITABLE_PROPERTY(type, name, setter, defaultValue) \
    protected: \
        Q_PROPERTY(type name MEMBER m_##name WRITE setter NOTIFY name##Changed) \
    public: \
        void setter(const type &value) { \
            if (value != m_##name) { \
                m_##name = value; \
                emit name##Changed(); \
            } \
        } \
    Q_SIGNALS: \
        void name##Changed(); \
    private: \
        type m_##name = defaultValue;

#define QML_READABLE_PROPERTY(type, name, defaultValue) \
    protected: \
        Q_PROPERTY(type name MEMBER m_##name NOTIFY name##Changed) \
    Q_SIGNALS: \
        void name##Changed(); \
    private: \
        type m_##name = defaultValue;

#define QML_CONSTANT_PROPERTY(type, name) \
    protected: \
        Q_PROPERTY(type name MEMBER m_##name CONSTANT) \
    private: \
        const type m_##name;

#define QML_WRITABLE_PROPERTY_FLOAT(type, name, setter, defaultValue) \
    protected: \
        Q_PROPERTY(type name MEMBER m_##name WRITE setter NOTIFY name##Changed) \
    public: \
        void setter(type value) { \
            if (!qFuzzyCompare(value, m_##name)) { \
                m_##name = value; \
                emit name##Changed(); \
            } \
        } \
    Q_SIGNALS: \
        void name##Changed(); \
    private: \
        type m_##name = defaultValue;

// NOTE : to avoid "no suitable class found" MOC note
class QmlProperty : public QObject { Q_OBJECT };

#endif
