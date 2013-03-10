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

#include "TestSuite/Tester.h"
#include "TestSuite/Compare/Container.h"
#include "Interconnect/Emitter.h"
#include "Interconnect/Receiver.h"

#include "corradeCompatibility.h"

namespace Corrade { namespace Interconnect { namespace Test {

class Test: public TestSuite::Tester {
    public:
        Test();

        void signalData();

        void connect();

        void disconnect();
        void disconnectSignal();
        void disconnectEmitter();
        void disconnectReceiver();

        void destroyEmitter();
        void destroyReceiver();

        void emit();
        void emitterSubclass();
        void receiverSubclass();
        void slotInReceiverBase();
        void virtualSlot();

        void changeConnectionsInSlot();
        void deleteReceiverInSlot();
};

class Postman: public Interconnect::Emitter {
    public:
        inline Signal newMessage(int price, const std::string& message) {
            return emit(&Postman::newMessage, price, message);
        }

        inline Signal paymentRequested(int amount) {
            return emit(&Postman::paymentRequested, amount);
        }
};

class Mailbox: public Interconnect::Receiver {
    public:
        inline Mailbox(): money(0) {}

        inline void addMessage(int price, const std::string& message) {
            money += price;
            messages.push_back(message);
        }

        inline void pay(int amount) {
            money -= amount;
        }

        int money;
        std::vector<std::string> messages;
};

Test::Test() {
    addTests({&Test::signalData,
              &Test::connect,
              &Test::disconnect,
              &Test::disconnectSignal,
              &Test::disconnectEmitter,
              &Test::disconnectReceiver,
              &Test::destroyEmitter,
              &Test::destroyReceiver,
              &Test::emit,
              &Test::emitterSubclass,
              &Test::receiverSubclass,
              &Test::slotInReceiverBase,
              &Test::virtualSlot,
              &Test::changeConnectionsInSlot,
              &Test::deleteReceiverInSlot});
}

void Test::signalData() {
    Implementation::SignalData data1(&Postman::newMessage);
    Implementation::SignalData data2(&Postman::newMessage);
    Implementation::SignalData data3(&Postman::paymentRequested);

    CORRADE_VERIFY(data1 == data1);
    CORRADE_VERIFY(data2 == data2);
    CORRADE_VERIFY(data3 == data3);
    CORRADE_VERIFY(data1 == data2);
    CORRADE_VERIFY(data1 != data3);
    CORRADE_VERIFY(data2 != data3);

    CORRADE_VERIFY(Implementation::SignalDataHash()(data1) == Implementation::SignalDataHash()(data1));
    CORRADE_VERIFY(Implementation::SignalDataHash()(data1) == Implementation::SignalDataHash()(data2));
    CORRADE_VERIFY(Implementation::SignalDataHash()(data1) != Implementation::SignalDataHash()(data3));
}

void Test::connect() {
    Postman postman;
    Mailbox mailbox1, mailbox2;

    /* Verify returned connection */
    Connection connection = Emitter::connect(&postman, &Postman::newMessage, &mailbox1, &Mailbox::addMessage);
    CORRADE_VERIFY(connection.isConnectionPossible());
    CORRADE_VERIFY(connection.isConnected());

    /* Verify connection adding */
    Emitter::connect(&postman, &Postman::paymentRequested, &mailbox1, &Mailbox::pay);
    Emitter::connect(&postman, &Postman::newMessage, &mailbox2, &Mailbox::addMessage);
    CORRADE_VERIFY(postman.hasSignalConnections());
    CORRADE_COMPARE(postman.signalConnectionCount(), 3);
    CORRADE_VERIFY(postman.hasSignalConnections(&Postman::newMessage));
    CORRADE_COMPARE(postman.signalConnectionCount(&Postman::newMessage), 2);
    CORRADE_VERIFY(postman.hasSignalConnections(&Postman::paymentRequested));
    CORRADE_COMPARE(postman.signalConnectionCount(&Postman::paymentRequested), 1);
    CORRADE_COMPARE(mailbox1.slotConnectionCount(), 2);
    CORRADE_COMPARE(mailbox2.slotConnectionCount(), 1);

    /* Allow multiple connections */
    Emitter::connect(&postman, &Postman::newMessage, &mailbox1, &Mailbox::addMessage);
    CORRADE_COMPARE(postman.signalConnectionCount(), 4);
    CORRADE_COMPARE(postman.signalConnectionCount(&Postman::newMessage), 3);
    CORRADE_COMPARE(mailbox1.slotConnectionCount(), 3);
}

void Test::disconnect() {
    Postman postman;
    Mailbox mailbox1, mailbox2;

    Connection connection = Emitter::connect(&postman, &Postman::newMessage, &mailbox1, &Mailbox::addMessage);
    Emitter::connect(&postman, &Postman::paymentRequested, &mailbox1, &Mailbox::pay);
    Emitter::connect(&postman, &Postman::newMessage, &mailbox2, &Mailbox::addMessage);

    /* Verify disconnection response */
    connection.disconnect();
    CORRADE_VERIFY(connection.isConnectionPossible());
    CORRADE_VERIFY(!connection.isConnected());
    CORRADE_COMPARE(postman.signalConnectionCount(&Postman::newMessage), 1);
    CORRADE_COMPARE(mailbox1.slotConnectionCount(), 1);

    /* Verify connection response */
    connection.connect();
    CORRADE_VERIFY(connection.isConnectionPossible());
    CORRADE_VERIFY(connection.isConnected());
    CORRADE_COMPARE(postman.signalConnectionCount(&Postman::newMessage), 2);
    CORRADE_COMPARE(mailbox1.slotConnectionCount(), 2);
}

void Test::disconnectSignal() {
    Postman postman;
    Mailbox mailbox1, mailbox2;

    Connection c1 = Emitter::connect(&postman, &Postman::newMessage, &mailbox1, &Mailbox::addMessage);
    Connection c2 = Emitter::connect(&postman, &Postman::newMessage, &mailbox2, &Mailbox::addMessage);
    Connection c3 = Emitter::connect(&postman, &Postman::paymentRequested, &mailbox1, &Mailbox::pay);

    postman.disconnectSignal(&Postman::newMessage);
    CORRADE_VERIFY(c1.isConnectionPossible());
    CORRADE_VERIFY(!c1.isConnected());
    CORRADE_VERIFY(c2.isConnectionPossible());
    CORRADE_VERIFY(!c2.isConnected());
    CORRADE_VERIFY(c3.isConnected());
    CORRADE_COMPARE(postman.signalConnectionCount(), 1);
    CORRADE_VERIFY(!postman.hasSignalConnections(&Postman::newMessage));
    CORRADE_COMPARE(postman.signalConnectionCount(&Postman::newMessage), 0);
    CORRADE_COMPARE(mailbox1.slotConnectionCount(), 1);
    CORRADE_COMPARE(mailbox2.slotConnectionCount(), 0);
}

void Test::disconnectEmitter() {
    Postman postman1, postman2;
    Mailbox mailbox;

    Connection c1 = Emitter::connect(&postman1, &Postman::newMessage, &mailbox, &Mailbox::addMessage);
    Connection c2 = Emitter::connect(&postman1, &Postman::paymentRequested, &mailbox, &Mailbox::pay);
    Connection c3 = Emitter::connect(&postman2, &Postman::newMessage, &mailbox, &Mailbox::addMessage);

    postman1.disconnectAllSignals();
    CORRADE_VERIFY(c1.isConnectionPossible());
    CORRADE_VERIFY(!c1.isConnected());
    CORRADE_VERIFY(c2.isConnectionPossible());
    CORRADE_VERIFY(!c2.isConnected());
    CORRADE_VERIFY(c3.isConnected());
    CORRADE_VERIFY(!postman1.hasSignalConnections());
    CORRADE_COMPARE(postman1.signalConnectionCount(), 0);
    CORRADE_VERIFY(postman2.hasSignalConnections());
    CORRADE_COMPARE(mailbox.slotConnectionCount(), 1);
}

void Test::disconnectReceiver() {
    Postman postman;
    Mailbox mailbox1, mailbox2;

    Connection c1 = Emitter::connect(&postman, &Postman::newMessage, &mailbox1, &Mailbox::addMessage);
    Connection c2 = Emitter::connect(&postman, &Postman::paymentRequested, &mailbox1, &Mailbox::pay);
    Connection c3 = Emitter::connect(&postman, &Postman::newMessage, &mailbox2, &Mailbox::addMessage);

    mailbox1.disconnectAllSlots();
    CORRADE_VERIFY(c1.isConnectionPossible());
    CORRADE_VERIFY(!c1.isConnected());
    CORRADE_VERIFY(c2.isConnectionPossible());
    CORRADE_VERIFY(!c2.isConnected());
    CORRADE_VERIFY(c3.isConnected());
    CORRADE_COMPARE(postman.signalConnectionCount(), 1);
    CORRADE_VERIFY(!mailbox1.hasSlotConnections());
    CORRADE_COMPARE(mailbox2.slotConnectionCount(), 1);
}

void Test::destroyEmitter() {
    Postman *postman1 = new Postman;
    Postman postman2;
    Mailbox mailbox;

    Connection c1 = Emitter::connect(postman1, &Postman::newMessage, &mailbox, &Mailbox::addMessage);
    Connection c2 = Emitter::connect(postman1, &Postman::paymentRequested, &mailbox, &Mailbox::pay);
    Connection c3 = Emitter::connect(&postman2, &Postman::newMessage, &mailbox, &Mailbox::addMessage);

    delete postman1;
    CORRADE_VERIFY(!c1.isConnectionPossible());
    CORRADE_VERIFY(!c1.isConnected());
    CORRADE_VERIFY(!c2.isConnectionPossible());
    CORRADE_VERIFY(!c2.isConnected());
    CORRADE_VERIFY(c3.isConnected());
    CORRADE_COMPARE(postman2.signalConnectionCount(), 1);
    CORRADE_COMPARE(mailbox.slotConnectionCount(), 1);
}

void Test::destroyReceiver() {
    Postman postman;
    Mailbox *mailbox1 = new Mailbox;
    Mailbox mailbox2;

    Connection c1 = Emitter::connect(&postman, &Postman::newMessage, mailbox1, &Mailbox::addMessage);
    Connection c2 = Emitter::connect(&postman, &Postman::paymentRequested, mailbox1, &Mailbox::pay);
    Connection c3 = Emitter::connect(&postman, &Postman::newMessage, &mailbox2, &Mailbox::addMessage);

    delete mailbox1;
    CORRADE_VERIFY(!c1.isConnectionPossible());
    CORRADE_VERIFY(!c1.isConnected());
    CORRADE_VERIFY(!c2.isConnectionPossible());
    CORRADE_VERIFY(!c2.isConnected());
    CORRADE_VERIFY(c3.isConnected());
    CORRADE_COMPARE(postman.signalConnectionCount(), 1);
    CORRADE_COMPARE(mailbox2.slotConnectionCount(), 1);
}

void Test::emit() {
    Postman postman;
    Mailbox mailbox1, mailbox2, mailbox3;
    Emitter::connect(&postman, &Postman::newMessage, &mailbox1, &Mailbox::addMessage);
    Emitter::connect(&postman, &Postman::newMessage, &mailbox2, &Mailbox::addMessage);
    Emitter::connect(&postman, &Postman::paymentRequested, &mailbox1, &Mailbox::pay);
    Emitter::connect(&postman, &Postman::paymentRequested, &mailbox2, &Mailbox::pay);
    Emitter::connect(&postman, &Postman::paymentRequested, &mailbox3, &Mailbox::pay);

    /* Verify signal handling */
    postman.newMessage(60, "hello");
    postman.paymentRequested(50);
    CORRADE_COMPARE(mailbox1.messages, std::vector<std::string>{"hello"});
    CORRADE_COMPARE(mailbox1.money, 10);
    CORRADE_COMPARE(mailbox2.messages, std::vector<std::string>{"hello"});
    CORRADE_COMPARE(mailbox2.money, 10);
    CORRADE_COMPARE(mailbox3.messages, std::vector<std::string>());
    CORRADE_COMPARE(mailbox3.money, -50);
}

void Test::emitterSubclass() {
    class BetterPostman: public Postman {
        public:
            inline Signal newRichTextMessage(int price, const std::string& value) {
                return emit(&BetterPostman::newRichTextMessage, price, "***"+value+"***");
            }
    };

    BetterPostman postman;
    Mailbox mailbox;

    /* Test that this doesn't spit any compiler errors */
    Emitter::connect(&postman, &BetterPostman::newRichTextMessage, &mailbox, &Mailbox::addMessage);
    Emitter::connect(&postman, &BetterPostman::newMessage, &mailbox, &Mailbox::addMessage);

    /* Just to be sure */
    postman.newMessage(5, "hello");
    postman.newRichTextMessage(10, "ahoy");
    CORRADE_COMPARE_AS(mailbox.messages, (std::vector<std::string>{"hello", "***ahoy***"}),
                       TestSuite::Compare::SortedContainer);
    CORRADE_COMPARE(mailbox.money, 15);

    postman.disconnectSignal(&BetterPostman::newMessage);
    CORRADE_VERIFY(postman.hasSignalConnections(&BetterPostman::newRichTextMessage));
    postman.disconnectSignal(&BetterPostman::newRichTextMessage);
    CORRADE_VERIFY(!postman.hasSignalConnections());
}

void Test::receiverSubclass() {
    class BlueMailbox: public Mailbox {
        public:
            void addBlueMessage(int price, const std::string& message) {
                money += price;
                messages.push_back("Blue " + message);
            }
    };

    Postman postman;
    BlueMailbox mailbox;

    /* Test that this doesn't spit any compiler errors */
    Emitter::connect(&postman, &Postman::newMessage, &mailbox, &BlueMailbox::addMessage);
    Emitter::connect(&postman, &Postman::newMessage, &mailbox, &BlueMailbox::addBlueMessage);

    /* Just to be sure */
    postman.newMessage(5, "hello");
    CORRADE_COMPARE_AS(mailbox.messages, (std::vector<std::string>{"Blue hello", "hello"}),
                       TestSuite::Compare::SortedContainer);
    CORRADE_COMPARE(mailbox.money, 10);
}

void Test::slotInReceiverBase() {
    class VintageMailbox {
        public:
            inline VintageMailbox(): money(0) {}

            void addMessage(int price, const std::string& message) {
                money += price;
                messages.push_back(message);
            }

            int money;
            std::vector<std::string> messages;
    };

    class ModernMailbox: public VintageMailbox, public Interconnect::Receiver {};

    Postman postman;
    ModernMailbox mailbox;

    /* Test that this doesn't spit any compiler errors */
    Emitter::connect(&postman, &Postman::newMessage, &mailbox, &VintageMailbox::addMessage);

    /* Just to be sure */
    postman.newMessage(5, "hello");
    CORRADE_COMPARE(mailbox.messages, std::vector<std::string>{"hello"});
    CORRADE_COMPARE(mailbox.money, 5);
}

void Test::virtualSlot() {
    class VirtualMailbox: public Interconnect::Receiver {
        public:
            inline VirtualMailbox(): money(0) {}

            inline void addMessage(int price, const std::string& message) {
                money += price;
                messages.push_back(message);
            }

            inline virtual void pay(int amount) {
                money -= amount;
            }

            int money;
            std::vector<std::string> messages;
    };

    class TaxDodgingMailbox: public VirtualMailbox {
        public:
            void pay(int amount) override {
                this->money -= amount/5;
            }
    };

    Postman postman;
    VirtualMailbox* mailbox = new TaxDodgingMailbox;

    /* It is important to connect to the original slot, not derived */
    Emitter::connect(&postman, &Postman::paymentRequested, mailbox, &VirtualMailbox::pay);

    postman.paymentRequested(50);
    CORRADE_COMPARE(mailbox->money, -10);

    delete mailbox;
}

void Test::changeConnectionsInSlot() {
    Postman postman;
    Mailbox mailbox;

    class PropagatingMailbox: public Interconnect::Receiver {
        public:
            inline PropagatingMailbox(Postman* postman, Mailbox* mailbox): postman(postman), mailbox(mailbox) {}

            inline void addMessage(int, const std::string& message) {
                this->messages.push_back(message);
                Emitter::connect(postman, &Postman::newMessage, mailbox, &Mailbox::addMessage);
                Emitter::connect(postman, &Postman::paymentRequested, mailbox, &Mailbox::pay);
            }

            std::vector<std::string> messages;

        private:
            Postman* postman;
            Mailbox* mailbox;
    };

    PropagatingMailbox propagatingMailbox(&postman, &mailbox);
    Emitter::connect(&postman, &Postman::newMessage, &propagatingMailbox, &PropagatingMailbox::addMessage);

    /* Not connected to anything */
    postman.paymentRequested(50);
    CORRADE_COMPARE(mailbox.money, 0);

    /* Propagating mailbox connects the other mailbox, verify the proper slots
       are called proper times */
    postman.newMessage(19, "hello");
    CORRADE_COMPARE(propagatingMailbox.messages, std::vector<std::string>{"hello"});
    CORRADE_COMPARE(mailbox.messages, std::vector<std::string>{"hello"});
    CORRADE_COMPARE(mailbox.money, 19);
}

void Test::deleteReceiverInSlot() {
    class SuicideMailbox: public Interconnect::Receiver {
        public:
            inline void addMessage(int, const std::string&) {
                delete this;
            }
    };

    Postman postman;
    SuicideMailbox* mailbox1 = new SuicideMailbox;
    Mailbox mailbox2, mailbox3;

    Emitter::connect(&postman, &Postman::newMessage, mailbox1, &SuicideMailbox::addMessage);
    Emitter::connect(&postman, &Postman::newMessage, &mailbox2, &Mailbox::addMessage);
    Emitter::connect(&postman, &Postman::newMessage, &mailbox3, &Mailbox::addMessage);

    /* Verify that the message is propagated to all slots */
    CORRADE_COMPARE(postman.signalConnectionCount(), 3);
    postman.newMessage(11, "hello");
    CORRADE_COMPARE(postman.signalConnectionCount(), 2);
    CORRADE_COMPARE(mailbox2.messages, std::vector<std::string>{"hello"});
    CORRADE_COMPARE(mailbox3.messages, std::vector<std::string>{"hello"});
}

}}}

CORRADE_TEST_MAIN(Corrade::Interconnect::Test::Test)
