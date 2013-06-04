#ifndef Corrade_Interconnect_Connection_h
#define Corrade_Interconnect_Connection_h
/*
    This file is part of Corrade.

    Copyright © 2007, 2008, 2009, 2010, 2011, 2012, 2013
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
 * @brief Class Corrade::Interconnect::Connection
 */

#include "Interconnect/Interconnect.h"
#include "Interconnect/corradeInterconnectVisibility.h"

#include <cstddef>

namespace Corrade { namespace Interconnect {

namespace Implementation {
    class AbstractConnectionData;

    class SignalData {
        friend class Interconnect::Emitter;
        friend class SignalDataHash;

        public:
            static const std::size_t Size = 2*sizeof(void*)/sizeof(std::size_t);

            template<class Emitter, class ...Args> SignalData(typename Emitter::Signal(Emitter::*signal)(Args...)): data() {
                typedef typename Emitter::Signal(Emitter::*Signal)(Args...);
                *reinterpret_cast<Signal*>(data) = signal;
            }

            bool operator==(const SignalData& other) const {
                for(std::size_t i = 0; i != Size; ++i)
                    if(data[i] != other.data[i]) return false;
                return true;
            }

            bool operator!=(const SignalData& other) const {
                return !operator==(other);
            }

        private:
            std::size_t data[Size];
    };
}

/**
@brief %Connection

Returned by Emitter::connect(), allows to remove or reestablish the connection.
Destruction of %Connection object does not remove the connection, after that
the only possibility to remove the connection is to disconnect whole emitter
or receiver or disconnect everything connected to given signal using
Emitter::disconnectSignal(), Emitter::disconnectAllSignals() or
Receiver::disconnectAllSlots() or destroy either emitter or receiver object.

@see @ref interconnect, Emitter, Receiver
*/
class CORRADE_INTERCONNECT_EXPORT Connection {
    friend class Emitter;
    friend class Receiver;

    public:
        /** @brief Copying is not allowed */
        Connection(const Connection&) = delete;

        /** @brief Move constructor */
        Connection(Connection&& other);

        /** @brief Copying is not allowed */
        Connection& operator=(const Connection&) = delete;

        /** @brief Move assignment */
        Connection& operator=(Connection&& other);

        /**
         * @brief Destructor
         *
         * Does not remove the connection.
         */
        ~Connection();

        /**
         * @brief Whether connection is possible
         * @return `False` if either emitter or receiver object doesn't exist
         *      anymore, `true` otherwise.
         *
         * @see isConnected()
         */
        bool isConnectionPossible() const { return data; }

        /**
         * @brief Whether the connection exists
         *
         * @see isConnectionPossible(), Emitter::hasSignalConnections(),
         *      Receiver::hasSlotConnections()
         */
        bool isConnected() const { return connected; }

        /**
         * @brief Establish the connection
         *
         * If connection is not possible, returns `false`, otherwise creates
         * the connection (if not already connected) and returns `true`.
         * @see isConnectionPossible(), isConnected(), Emitter::connect()
         */
        bool connect();

        /**
         * @brief Remove the connection
         *
         * Disconnects if connection exists.
         * @see isConnected(), Emitter::disconnectSignal(),
         *      Emitter::disconnectAllSignals(), Receiver::disconnectAllSlots()
         */
        void disconnect();

    private:
        explicit Connection(Implementation::SignalData signal, Implementation::AbstractConnectionData* data);

        void destroy();
        void move(Connection&& other);

        Implementation::SignalData signal;
        Implementation::AbstractConnectionData* data;
        bool connected;
};

}}

#endif
