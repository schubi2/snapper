/*
 * Copyright (c) 2012 Novell, Inc.
 * Copyright (c) 2016 SUSE LLC
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#ifndef SNAPPER_DBUSMESSAGE_H
#define SNAPPER_DBUSMESSAGE_H


#include <dbus/dbus.h>

#include <string>
#include <vector>
#include <list>
#include <map>

#include "snapper/Exception.h"


namespace DBus
{
    using std::string;
    using std::vector;
    using std::list;
    using std::map;


    struct Exception : public snapper::Exception
    {
	explicit Exception() : snapper::Exception("dbus generic exception") {}
	explicit Exception(const string& msg) : snapper::Exception(msg) {}
    };


    struct ErrorException : public Exception
    {
	explicit ErrorException(const DBusError err)
	    : Exception("dbus error exception"), err(err) {}
	virtual ~ErrorException() throw() { dbus_error_free(&err); }
	virtual const char* name() const throw() { return err.name; }
	virtual const char* message() const throw() { return err.message; }
	DBusError err;
    };


    struct MarshallingException : public Exception
    {
	explicit MarshallingException() : Exception("dbus marshalling exception") {}
    };


    struct FatalException : public Exception
    {
	explicit FatalException() : Exception("dbus fatal exception") {}
    };


    class Message
    {
    public:

	Message(DBusMessage* m, bool ref);
	Message(const Message& m);
	~Message();

	Message& operator=(const Message& m);

	DBusMessage* get_message() { return msg; }

	int get_type() const { return dbus_message_get_type(msg); }
	string get_member() const { return dbus_message_get_member(msg); }
	string get_sender() const { return dbus_message_get_sender(msg); }
	string get_path() const { return dbus_message_get_path(msg); }
	string get_interface() const { return dbus_message_get_interface(msg); }
	string get_error_name() const { return dbus_message_get_error_name(msg); }

	bool is_method_call(const char* interface, const char* method) const
	{
	    return dbus_message_is_method_call(msg, interface, method);
	}

	bool is_signal(const char* interface, const char* name) const
	{
	    return dbus_message_is_signal(msg, interface, name);
	}

    private:

	DBusMessage* msg;

    };


    class MessageMethodCall : public Message
    {
    public:

	MessageMethodCall(const char* service, const char* object, const char* interface,
			  const char* method)
	    : Message(dbus_message_new_method_call(service, object, interface, method), false)
	{
	}

    };


    class MessageMethodReturn : public Message
    {
    public:

	MessageMethodReturn(Message& m)
	    : Message(dbus_message_new_method_return(m.get_message()), false)
	{
	    if (m.get_type() != DBUS_MESSAGE_TYPE_METHOD_CALL)
		throw FatalException();
	}

    };


    class MessageError : public Message
    {
    public:

	MessageError(Message& m, const char* error_msg, const char* error_code)
	    : Message(dbus_message_new_error(m.get_message(), error_msg, error_code), false)
	{
	    if (m.get_type() != DBUS_MESSAGE_TYPE_METHOD_CALL)
		throw FatalException();
	}

    };


    class MessageSignal : public Message
    {
    public:

	MessageSignal(const char* path, const char* interface, const char* name)
	    : Message(dbus_message_new_signal(path, interface, name), false)
	{
	}

    };


    template <typename Type> struct TypeInfo {};

    template <> struct TypeInfo<dbus_uint32_t> { static const char* signature; };
    template <> struct TypeInfo<dbus_uint64_t> { static const char* signature; };
    template <> struct TypeInfo<string> { static const char* signature; };


    class Marshalling
    {

    public:

	DBusMessageIter* top() { return iters.back(); }

	int get_type() { return dbus_message_iter_get_arg_type(top()); }
	string get_signature() { return dbus_message_iter_get_signature(top()); }

    protected:

	vector<DBusMessageIter*> iters;

    };


    class Unmarshaller : public Marshalling
    {

    public:

    Unmarshaller(Message& msg);
    ~Unmarshaller();

	void open_recurse();
	void close_recurse();

	static string unescape(const string&);

    };


    class Marshaller : public Marshalling
    {

    public:

    Marshaller(Message& msg);
    ~Marshaller();

	void open_struct();
	void close_struct();

	void open_array(const char* signature);
	void close_array();

	void open_dict_entry();
	void close_dict_entry();

	static string escape(const string&);

    };


    Unmarshaller& operator>>(Unmarshaller& unmarshaller, bool& data);
    Marshaller& operator<<(Marshaller& marshaller, bool data);

    Unmarshaller& operator>>(Unmarshaller& unmarshaller, dbus_uint16_t& data);
    Marshaller& operator<<(Marshaller& marshaller, dbus_uint16_t data);

    Unmarshaller& operator>>(Unmarshaller& unmarshaller, dbus_uint32_t& data);
    Marshaller& operator<<(Marshaller& marshaller, dbus_uint32_t data);

    Unmarshaller& operator>>(Unmarshaller& unmarshaller, dbus_uint64_t& data);
    Marshaller& operator<<(Marshaller& marshaller, dbus_uint64_t data);

    Unmarshaller& operator>>(Unmarshaller& unmarshaller, time_t& data);
    Marshaller& operator<<(Marshaller& marshaller, time_t data);

    Marshaller& operator<<(Marshaller& marshaller, const char* data);

    Unmarshaller& operator>>(Unmarshaller& unmarshaller, string& data);
    Marshaller& operator<<(Marshaller& marshaller, const string& data);

    Unmarshaller& operator>>(Unmarshaller& unmarshaller, map<string, string>& data);
    Marshaller& operator<<(Marshaller& marshaller, const map<string, string>& data);


    template <typename Type>
    Unmarshaller& operator>>(Unmarshaller& unmarshaller, vector<Type>& data)
    {
    if (unmarshaller.get_type() != DBUS_TYPE_ARRAY)
	    throw MarshallingException();

    unmarshaller.open_recurse();

    while (unmarshaller.get_type() != DBUS_TYPE_INVALID)
	{
        if (unmarshaller.get_signature() != TypeInfo<Type>::signature)
		throw MarshallingException();

	    Type tmp;
        unmarshaller >> tmp;
	    data.push_back(tmp);
	}

    unmarshaller.close_recurse();

    return unmarshaller;
    }


    template <typename Type>
    Marshaller& operator<<(Marshaller& marshaller, const vector<Type>& data)
    {
    marshaller.open_array(TypeInfo<Type>::signature);

	for (typename vector<Type>::const_iterator it = data.begin(); it != data.end(); ++it)
	{
        marshaller << *it;
	}

    marshaller.close_array();

    return marshaller;
    }


    template <typename Type>
    Unmarshaller& operator>>(Unmarshaller& unmarshaller, list<Type>& data)
    {
    if (unmarshaller.get_type() != DBUS_TYPE_ARRAY)
	    throw MarshallingException();

    unmarshaller.open_recurse();

    while (unmarshaller.get_type() != DBUS_TYPE_INVALID)
	{
        if (unmarshaller.get_signature() != TypeInfo<Type>::signature)
		throw MarshallingException();

	    Type tmp;
        unmarshaller >> tmp;
	    data.push_back(tmp);
	}

    unmarshaller.close_recurse();

    return unmarshaller;
    }


    template <typename Type>
    Marshaller& operator<<(Marshaller& marshaller, const list<Type>& data)
    {
    marshaller.open_array(TypeInfo<Type>::signature);

	for (typename list<Type>::const_iterator it = data.begin(); it != data.end(); ++it)
	{
        marshaller << *it;
	}

    marshaller.close_array();

    return marshaller;
    }

}


#endif
