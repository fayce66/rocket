/***********************************************************************************
 * rocket - lightweight & fast signal/slots & utility library                      *
 *                                                                                 *
 *   v2.0 - public domain                                                          *
 *   no warranty is offered or implied; use this code at your own risk             *
 *                                                                                 *
 * AUTHORS                                                                         *
 *                                                                                 *
 *   Written by Michael Bleis                                                      *
 *                                                                                 *
 *                                                                                 *
 * LICENSE                                                                         *
 *                                                                                 *
 *   This software is dual-licensed to the public domain and under the following   *
 *   license: you are granted a perpetual, irrevocable license to copy, modify,    *
 *   publish, and distribute this file as you see fit.                             *
 ***********************************************************************************/

#ifndef ROCKET_HPP_INCLUDED
#define ROCKET_HPP_INCLUDED

/***********************************************************************************
 * CONFIGURATION                                                                   *
 * ------------------------------------------------------------------------------- *
 * Define this if your compiler doesn't support std::optional.                     *
 * ------------------------------------------------------------------------------- */

#ifndef ROCKET_NO_STD_OPTIONAL
// #define ROCKET_NO_STD_OPTIONAL
#endif

/***********************************************************************************
 * ------------------------------------------------------------------------------- *
 * Define this if you want to disable exceptions.                                  *
 * ------------------------------------------------------------------------------- */

#ifndef ROCKET_NO_EXCEPTIONS
// #define ROCKET_NO_EXCEPTIONS
#endif

/***********************************************************************************
 * ------------------------------------------------------------------------------- *
 * Redefine this if your compiler doesn't support the `thread_local`-keyword.      *
 * For Visual Studio < 2015 you can define it to `__declspec(thread)` for example. *
 * ------------------------------------------------------------------------------- */

#ifndef ROCKET_THREAD_LOCAL
#define ROCKET_THREAD_LOCAL thread_local
#endif

/***********************************************************************************
 * ------------------------------------------------------------------------------- *
 * Redefine this if your compiler doesn't support the `noexcept`-keyword.          *
 * For Visual Studio < 2015 you can define it to `throw()` for example.            *
 * ------------------------------------------------------------------------------- */

#ifndef ROCKET_NOEXCEPT
#define ROCKET_NOEXCEPT noexcept
#endif


/***********************************************************************************
 * USAGE                                                                           *
 * ------------------------------------------------------------------------------- *
 * 1. Creating your first signal                                                   *
 * ------------------------------------------------------------------------------- *

#include <iostream>

int main() {
    rocket::signal<void()> my_signal;

    // Connecting the first handler to our signal
    my_signal.connect([]() {
        std::cout << "First handler called!" << std::endl;
    });

    // Connecting a second handler to our signal using alternative syntax
    my_signal += []() {
        std::cout << "Second handler called!" << std::endl;
    };

    // Invoking the signal
    my_signal();
}

// Output:
//     First handler called!
//     Second handler called!

 * ------------------------------------------------------------------------------- *
 * 2. Passing arguments to the signal                                              *
 * ------------------------------------------------------------------------------- *

#include <string>
#include <iostream>

int main() {
    rocket::signal<void(std::string)> my_signal;

    my_signal.connect([](const std::string& argument) {
        std::cout << "Handler called with arg: " << argument << std::endl;
    });

    my_signal("Hello world");
}

// Output:
//     Handler called with arg: Hello world

 * ------------------------------------------------------------------------------- *
 * 3. Connecting class methods to the signal                                       *
 * ------------------------------------------------------------------------------- *

#include <string>
#include <iostream>

class Subject {
public:
    void setName(const std::string& newName) {
        if (name != newName) {
            name = newName;
            nameChanged(newName);
        }
    }

public:
    rocket::signal<void(std::string)> nameChanged;

private:
    std::string name;
};

class Observer {
public:
    Observer(Subject& subject) {
        // Register the `onNameChanged`-function of this object as a listener and
        // store the resultant connection object in the listener's connection set.

        // This is all your need to do for the most common case, if you want the
        // connection to be broken when the observer is destroyed.
        connections += {
            subject.nameChanged.connect(this, &Observer::onNameChanged)
        };
    }

    void onNameChanged(const std::string& name) {
        std::cout << "Subject received new name: " << name << std::endl;
    }

private:
    rocket::scoped_connection_container connections;
};

int main() {
    Subject s;
    Observer o{ s };
    s.setName("Peter");
}

// Output:
//     Subject received new name: Peter


#include <string>
#include <iostream>
#include <memory>

struct ILogger {
    virtual void logMessage(const std::string& message) = 0;
};

struct ConsoleLogger : ILogger {
    void logMessage(const std::string& message) override {
        std::cout << "New log message: " << message << std::endl;
    }
};

struct App {
    void run() {
        if (work()) {
            onSuccess("I finished my work!");
        }
    }
    bool work() {
        return true;
    }
    rocket::signal<void(std::string)> onSuccess;
};

int main() {
    std::unique_ptr<App> app = std::make_unique<App>();

    std::unique_ptr<ILogger> logger = std::make_unique<ConsoleLogger>();
    app->onSuccess.connect(logger.get(), &ILogger::logMessage);

    app->run();
}

// Output:
//     New log message: I finished my work!

 * ------------------------------------------------------------------------------- *
 * 4.a Handling lifetime and scope of connection objects                            *
 *                                                                                 *
 * What if we want to destroy our logger instance from example 3 but continue      *
 * to use the app instance?                                                        *
 *                                                                                 *
 * Solution: We use `scoped_connection`-objects to track our connected slots!      *
 * ------------------------------------------------------------------------------- *

// [...] (See example 3)

int main() {
    std::unique_ptr<App> app = std::make_unique<App>();
    {
        std::unique_ptr<ILogger> logger = std::make_unique<ConsoleLogger>();

        rocket::scoped_connection connection = app->onSuccess
            .connect(logger.get(), &ILogger::logMessage);

        app->run();

    } //<-- `logger`-instance is destroyed at the end of this block
      //<-- The `connection`-object is also destroyed here
      //        and therefore removed from App::onSuccess.

    // Run the app a second time
    //
    // This would normally cause a crash / undefined behavior because the logger
    // instance is destroyed at this point, but App::onSuccess still referenced it
    // in example 3.

    app->run();
}

// Output:
//     New log message: I finished my work!

 * ------------------------------------------------------------------------------- *
 * 4.b Advanced lifetime tracking                                                  *
 *                                                                                 *
 * The library can also track the lifetime of your class objects for you, if the   *
 * connected slot instances inherit from the `rocket::trackable` base class.       *
 * ------------------------------------------------------------------------------- *

 // [...] (See example 3)

struct ILogger : rocket::trackable {
    virtual void logMessage(const std::string& message) = 0;
};

// [...] (See example 3)

int main() {
    std::unique_ptr<App> app = std::make_unique<App>();
    {
        std::unique_ptr<ILogger> logger = std::make_unique<ConsoleLogger>();
        app->onSuccess.connect(logger.get(), &ILogger::logMessage);

        app->run();

    } //<-- `logger`-instance is destroyed at the end of this block

      //<-- Because `ILogger` inherits from `rocket::trackable`, the signal knows
      //        about its destruction and will automatically disconnect the slot!

    // Run the app a second time
    //
    // This would normally cause a crash / undefined behavior because the logger
    // instance is destroyed at this point, but App::onSuccess still referenced it
    // in example 3.

    app->run();
}

 * ------------------------------------------------------------------------------- *
 * 5. Getting return values from a call to a signal                                *
 *                                                                                 *
 * Slots can also return values to the emitting signal.                            *
 * Because a signal can have several slots attached to it, the return values are   *
 * collected by using the so called `value collectors`.                            *
 *                                                                                 *
 * The default value collector returns an `optional<T>` from a call to a           *
 * `signal<T(...)>::operator()`                                                    *
 *                                                                                 *
 * However, this behaviour can be overriden at declaration time of the signal as   *
 * well as during signal invocation.                                               *
 * ------------------------------------------------------------------------------- *

#include <cmath>
#include <iostream>

int main() {
    rocket::signal<int(int)> signal;

    // The library supports argument and return type transformation between the
    // signal and the slots. We show this by connecting the `float sqrtf(float)`
    // function to a signal with an `int` argument and `int` return value.

    signal.connect(std::sqrtf);

    std::cout << "Computed value: " << *signal(16);
}

// Output:
//     Computed value: 4

#include <cmath>
#include <iostream>
#include <iomanip>

int main() {
    // Because we set `rocket::range` as the value collector for this signal
    // calling operator() now returns the return values of all connected slots.

    rocket::signal<float(float), rocket::range<float>> signal;

    // Lets connect a couple more functions to our signal and print all the
    // return values.

    signal.connect(std::sinf);
    signal.connect(std::cosf);

    std::cout << std::fixed << std::setprecision(2);

    for (auto result : signal(3.14159)) {
        std::cout << result << std::endl;
    }

    // We can also override the return value collector at invocation time

    std::cout << "First return value: " << signal.invoke<rocket::first<float>>(3.14159);
    std::cout << std::endl;
    std::cout << "Last return value: " << signal.invoke<rocket::last<float>>(3.14159);
}

// Output:
//     0.00
//     -1.00
//     First return value: 0.00
//     Last return value: -1.00

 * ------------------------------------------------------------------------------- *
 * 6. Accessing the current connection object inside a slot                        *
 *                                                                                 *
 * Sometimes it is desirable to get an instance to the current connection object   *
 * inside of a slot function. An example would be if you want to make a callback   *
 * that only fires once and then disconnects itself from the signal that called it *
 * ------------------------------------------------------------------------------- *

#include <iostream>

int main() {
    rocket::signal<void()> signal;

    signal.connect([] {
        std::cout << "Slot called. Now disconnecting..." << std::endl;

        // `current_connection` is stored in thread-local-storage.
        rocket::current_connection().disconnect();
    });

    signal();
    signal();
    signal();
}

// Output:
//     Slot called. Now disconnecting...

 * ------------------------------------------------------------------------------- *
 * 7. Preemtively aborting the emission of a signal                                *
 *                                                                                 *
 * A slot can preemtively abort the emission of a signal if it needs to.           *
 * This is useful in scenarios where your slot functions try to find some value    *
 * and you just want the result of the first slot that found one and stop other    *
 * slots from running.                                                             *
 * ------------------------------------------------------------------------------- *

#include <iostream>

int main() {
    rocket::signal<void()> signal;

    signal.connect([] {
        std::cout << "First slot called. Aborting emission of other slots." << std::endl;

        rocket::abort_emission();

        // Notice that this doesn't disconnect the other slots. It just breaks out of the
        // signal emitting loop.
    });

    signal.connect([] {
        std::cout << "Second slot called. Should never happen." << std::endl;
    });

    signal();
}

// Output:
//     First slot called. Aborting emission of other slots.














 ***********************************************************************************
 * BEGIN IMPLEMENTATION                                                            *
 * ------------------------------------------------------------------------------- *
 * Do not change anything below this line                                          *
 * ------------------------------------------------------------------------------- *
 ***********************************************************************************/

#include <iterator>
#include <exception>
#include <type_traits>
#include <cassert>
#include <utility>
#include <memory>
#include <functional>
#include <list>
#include <forward_list>
#include <initializer_list>
#include <atomic>
#include <limits>
#include <mutex>
#include <future>
#include <unordered_map>
#include <deque>

#ifndef ROCKET_NO_STD_OPTIONAL
#   include <optional>
#endif

namespace rocket
{
    template <class T>
    struct minimum
    {
        using value_type = T;
        using result_type = T;

        template <class U>
        void operator () (U&& value)
        {
            if (!has_value || value < current) {
                current = std::forward<U>(value);
                has_value = true;
            }
        }

        result_type result()
        {
            return std::move(current);
        }

    private:
        value_type current{};
        bool has_value{ false };
    };

    template <class T>
    struct maximum
    {
        using value_type = T;
        using result_type = T;

        template <class U>
        void operator () (U&& value)
        {
            if (!has_value || value > current) {
                current = std::forward<U>(value);
                has_value = true;
            }
        }

        result_type result()
        {
            return std::move(current);
        }

    private:
        value_type current{};
        bool has_value{ false };
    };

    template <class T>
    struct first
    {
        using value_type = T;
        using result_type = T;

        template <class U>
        void operator () (U&& value)
        {
            if (!has_value) {
                current = std::forward<U>(value);
                has_value = true;
            }
        }

        result_type result()
        {
            return std::move(current);
        }

    private:
        value_type current{};
        bool has_value{ false };
    };

    template <class T>
    struct last
    {
        using value_type = T;
        using result_type = T;

        template <class U>
        void operator () (U&& value)
        {
            current = std::forward<U>(value);
        }

        result_type result()
        {
            return std::move(current);
        }

    private:
        value_type current{};
    };

    template <class T>
    struct range
    {
        using value_type = T;
        using result_type = std::list<T>;

        template <class U>
        void operator () (U&& value)
        {
            values.emplace_back(std::forward<U>(value));
        }

        result_type result()
        {
            return std::move(values);
        }

    private:
        std::list<value_type> values;
    };

#ifndef ROCKET_NO_EXCEPTIONS
    struct error : std::exception
    {
    };

    struct bad_optional_access : error
    {
        const char* what() const ROCKET_NOEXCEPT override
        {
            return "rocket: Bad optional access.";
        }
    };

    struct invocation_slot_error : error
    {
        const char* what() const ROCKET_NOEXCEPT override
        {
            return "rocket: One of the slots has raised an exception during the signal invocation.";
        }
    };
#endif

#ifdef ROCKET_NO_STD_OPTIONAL
    template <class T>
    struct optional
    {
        using value_type = T;

        optional() ROCKET_NOEXCEPT = default;

        ~optional() ROCKET_NOEXCEPT
        {
            if (engaged()) {
                disengage();
            }
        }

        template <class... Args>
        explicit optional(Args&&... args)
        {
            engage(std::forward<Args>(args)...);
        }

        optional(optional const& opt)
        {
            if (opt.engaged()) {
                engage(*opt.object());
            }
        }

        optional(optional&& opt)
        {
            if (opt.engaged()) {
                engage(std::move(*opt.object()));
                opt.disengage();
            }
        }

        template <class U>
        explicit optional(optional<U> const& opt)
        {
            if (opt.engaged()) {
                engage(*opt.object());
            }
        }

        template <class U>
        explicit optional(optional<U>&& opt)
        {
            if (opt.engaged()) {
                engage(std::move(*opt.object()));
                opt.disengage();
            }
        }

        template <class U>
        optional& operator = (U&& rhs)
        {
            if (engaged()) {
                disengage();
            }
            engage(std::forward<U>(rhs));
            return *this;
        }

        optional& operator = (optional const& rhs)
        {
            if (this != &rhs) {
                if (engaged()) {
                    disengage();
                }
                if (rhs.engaged()) {
                    engage(*rhs.object());
                }
            }
            return *this;
        }

        template <class U>
        optional& operator = (optional<U> const& rhs)
        {
            if (this != &rhs) {
                if (engaged()) {
                    disengage();
                }
                if (rhs.engaged()) {
                    engage(*rhs.object());
                }
            }
            return *this;
        }

        optional& operator = (optional&& rhs)
        {
            if (engaged()) {
                disengage();
            }
            if (rhs.engaged()) {
                engage(std::move(*rhs.object()));
                rhs.disengage();
            }
            return *this;
        }

        template <class U>
        optional& operator = (optional<U>&& rhs)
        {
            if (engaged()) {
                disengage();
            }
            if (rhs.engaged()) {
                engage(std::move(*rhs.object()));
                rhs.disengage();
            }
            return *this;
        }

        void reset() ROCKET_NOEXCEPT
        {
            if (engaged()) {
                disengage();
            }
        }

        template <class... Args>
        value_type& emplace(Args&&... args)
        {
            if (engaged()) {
                disengage();
            }
            engage(std::forward<Args>(args)...);
            return value();
        }

        bool engaged() const ROCKET_NOEXCEPT
        {
            return initialized;
        }

        bool has_value() const ROCKET_NOEXCEPT
        {
            return initialized;
        }

        explicit operator bool() const ROCKET_NOEXCEPT
        {
            return engaged();
        }

        value_type& operator * () ROCKET_NOEXCEPT
        {
            return value();
        }

        value_type const& operator * () const ROCKET_NOEXCEPT
        {
            return value();
        }

        value_type* operator -> () ROCKET_NOEXCEPT
        {
            return object();
        }

        value_type const* operator -> () const ROCKET_NOEXCEPT
        {
            return object();
        }

        value_type& value()
        {
#ifndef ROCKET_NO_EXCEPTIONS
            if (!engaged()) {
                throw bad_optional_access{};
            }
#endif
            return *object();
        }

        value_type const& value() const
        {
#ifndef ROCKET_NO_EXCEPTIONS
            if (!engaged()) {
                throw bad_optional_access{};
            }
#endif
            return *object();
        }

        template <class U>
        value_type value_or(U&& val) const
        {
            return engaged() ? *object() : value_type{ std::forward<U>(val) };
        }

        void swap(optional& other)
        {
            if (this != &other) {
                auto t{ std::move(*this) };
                *this = std::move(other);
                other = std::move(t);
            }
        }

    private:
        void* storage() ROCKET_NOEXCEPT
        {
            return static_cast<void*>(&buffer);
        }

        void const* storage() const ROCKET_NOEXCEPT
        {
            return static_cast<void const*>(&buffer);
        }

        value_type* object() ROCKET_NOEXCEPT
        {
            assert(initialized == true);
            return static_cast<value_type*>(storage());
        }

        value_type const* object() const ROCKET_NOEXCEPT
        {
            assert(initialized == true);
            return static_cast<value_type const*>(storage());
        }

        template <class... Args>
        void engage(Args&&... args)
        {
            assert(initialized == false);
            new (storage()) value_type{ std::forward<Args>(args)... };
            initialized = true;
        }

        void disengage() ROCKET_NOEXCEPT
        {
            assert(initialized == true);
            object()->~value_type();
            initialized = false;
        }

        bool initialized = false;
        std::aligned_storage_t<sizeof(value_type), std::alignment_of<value_type>::value> buffer;
    };
#else
    template <class T> using optional = std::optional<T>;
#endif

    template <class T>
    struct intrusive_ptr
    {
        using value_type = T;
        using element_type = T;
        using pointer = T*;
        using reference = T&;

        template <class U> friend struct intrusive_ptr;

        intrusive_ptr() ROCKET_NOEXCEPT
            : ptr{ nullptr }
        {
        }

        explicit intrusive_ptr(std::nullptr_t) ROCKET_NOEXCEPT
            : ptr{ nullptr }
        {
        }

        explicit intrusive_ptr(pointer p) ROCKET_NOEXCEPT
            : ptr{ p }
        {
            if (ptr) {
                ptr->addref();
            }
        }

        intrusive_ptr(intrusive_ptr const& p) ROCKET_NOEXCEPT
            : ptr{ p.ptr }
        {
            if (ptr) {
                ptr->addref();
            }
        }

        intrusive_ptr(intrusive_ptr&& p) ROCKET_NOEXCEPT
            : ptr{ p.ptr }
        {
            p.ptr = nullptr;
        }

        template <class U>
        explicit intrusive_ptr(intrusive_ptr<U> const& p) ROCKET_NOEXCEPT
            : ptr{ p.ptr }
        {
            if (ptr) {
                ptr->addref();
            }
        }

        template <class U>
        explicit intrusive_ptr(intrusive_ptr<U>&& p) ROCKET_NOEXCEPT
            : ptr{ p.ptr }
        {
            p.ptr = nullptr;
        }

        ~intrusive_ptr() ROCKET_NOEXCEPT
        {
            if (ptr) {
                ptr->release();
            }
        }

        pointer get() const ROCKET_NOEXCEPT
        {
            return ptr;
        }

        pointer get() const volatile ROCKET_NOEXCEPT
        {
            return ptr;
        }

        pointer detach() ROCKET_NOEXCEPT
        {
            pointer p = ptr;
            ptr = nullptr;
            return p;
        }

        operator pointer() const ROCKET_NOEXCEPT
        {
            return ptr;
        }

        operator pointer() const volatile ROCKET_NOEXCEPT
        {
            return ptr;
        }

        pointer operator -> () const ROCKET_NOEXCEPT
        {
            assert(ptr != nullptr);
            return ptr;
        }

        reference operator * () const ROCKET_NOEXCEPT
        {
            assert(ptr != nullptr);
            return *ptr;
        }

        pointer* operator & () ROCKET_NOEXCEPT
        {
            assert(ptr == nullptr);
            return &ptr;
        }

        pointer const* operator & () const ROCKET_NOEXCEPT
        {
            return &ptr;
        }

        intrusive_ptr& operator = (pointer p) ROCKET_NOEXCEPT
        {
            if (p) {
                p->addref();
            }
            pointer o = ptr;
            ptr = p;
            if (o) {
                o->release();
            }
            return *this;
        }

        intrusive_ptr& operator = (std::nullptr_t) ROCKET_NOEXCEPT
        {
            if (ptr) {
                ptr->release();
                ptr = nullptr;
            }
            return *this;
        }

        intrusive_ptr volatile& operator = (std::nullptr_t) volatile ROCKET_NOEXCEPT
        {
            if (ptr) {
                ptr->release();
                ptr = nullptr;
            }
            return *this;
        }

        intrusive_ptr& operator = (intrusive_ptr const& p) ROCKET_NOEXCEPT
        {
            return (*this = p.ptr);
        }

        intrusive_ptr& operator = (intrusive_ptr&& p) ROCKET_NOEXCEPT
        {
            if (ptr) {
                ptr->release();
            }
            ptr = p.ptr;
            p.ptr = nullptr;
            return *this;
        }

        template <class U>
        intrusive_ptr& operator = (intrusive_ptr<U> const& p) ROCKET_NOEXCEPT
        {
            return (*this = p.ptr);
        }

        template <class U>
        intrusive_ptr& operator = (intrusive_ptr<U>&& p) ROCKET_NOEXCEPT
        {
            if (ptr) {
                ptr->release();
            }
            ptr = p.ptr;
            p.ptr = nullptr;
            return *this;
        }

        void swap(pointer* pp) ROCKET_NOEXCEPT
        {
            pointer p = ptr;
            ptr = *pp;
            *pp = p;
        }

        void swap(intrusive_ptr& p) ROCKET_NOEXCEPT
        {
            swap(&p.ptr);
        }

    private:
        pointer ptr;
    };

    template <class T, class U>
    bool operator == (intrusive_ptr<T> const& a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a.get() == b.get();
    }

    template <class T, class U>
    bool operator == (intrusive_ptr<T> const& a, U* b) ROCKET_NOEXCEPT
    {
        return a.get() == b;
    }

    template <class T, class U>
    bool operator == (T* a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a == b.get();
    }

    template <class T, class U>
    bool operator != (intrusive_ptr<T> const& a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a.get() != b.get();
    }

    template <class T, class U>
    bool operator != (intrusive_ptr<T> const& a, U* b) ROCKET_NOEXCEPT
    {
        return a.get() != b;
    }

    template <class T, class U>
    bool operator != (T* a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a != b.get();
    }

    template <class T, class U>
    bool operator < (intrusive_ptr<T> const& a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a.get() < b.get();
    }

    template <class T, class U>
    bool operator < (intrusive_ptr<T> const& a, U* b) ROCKET_NOEXCEPT
    {
        return a.get() < b;
    }

    template <class T, class U>
    bool operator < (T* a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a < b.get();
    }

    template <class T, class U>
    bool operator <= (intrusive_ptr<T> const& a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a.get() <= b.get();
    }

    template <class T, class U>
    bool operator <= (intrusive_ptr<T> const& a, U* b) ROCKET_NOEXCEPT
    {
        return a.get() <= b;
    }

    template <class T, class U>
    bool operator <= (T* a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a <= b.get();
    }

    template <class T, class U>
    bool operator > (intrusive_ptr<T> const& a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a.get() > b.get();
    }

    template <class T, class U>
    bool operator > (intrusive_ptr<T> const& a, U* b) ROCKET_NOEXCEPT
    {
        return a.get() > b;
    }

    template <class T, class U>
    bool operator > (T* a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a > b.get();
    }

    template <class T, class U>
    bool operator >= (intrusive_ptr<T> const& a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a.get() >= b.get();
    }

    template <class T, class U>
    bool operator >= (intrusive_ptr<T> const& a, U* b) ROCKET_NOEXCEPT
    {
        return a.get() >= b;
    }

    template <class T, class U>
    bool operator >= (T* a, intrusive_ptr<U> const& b) ROCKET_NOEXCEPT
    {
        return a >= b.get();
    }

    template <class T>
    bool operator == (intrusive_ptr<T> const& a, std::nullptr_t) ROCKET_NOEXCEPT
    {
        return a.get() == nullptr;
    }

    template <class T>
    bool operator == (std::nullptr_t, intrusive_ptr<T> const& b) ROCKET_NOEXCEPT
    {
        return nullptr == b.get();
    }

    template <class T>
    bool operator != (intrusive_ptr<T> const& a, std::nullptr_t) ROCKET_NOEXCEPT
    {
        return a.get() != nullptr;
    }

    template <class T>
    bool operator != (std::nullptr_t, intrusive_ptr<T> const& b) ROCKET_NOEXCEPT
    {
        return nullptr != b.get();
    }

    template <class T>
    T* get_pointer(intrusive_ptr<T> const& p) ROCKET_NOEXCEPT
    {
        return p.get();
    }

    template <class T, class U>
    intrusive_ptr<U> static_pointer_cast(intrusive_ptr<T> const& p) ROCKET_NOEXCEPT
    {
        return intrusive_ptr<U>{ static_cast<U*>(p.get()) };
    }

    template <class T, class U>
    intrusive_ptr<U> const_pointer_cast(intrusive_ptr<T> const& p) ROCKET_NOEXCEPT
    {
        return intrusive_ptr<U>{ const_cast<U*>(p.get()) };
    }

    template <class T, class U>
    intrusive_ptr<U> dynamic_pointer_cast(intrusive_ptr<T> const& p) ROCKET_NOEXCEPT
    {
        return intrusive_ptr<U>{ dynamic_cast<U*>(p.get()) };
    }

    struct ref_count
    {
        unsigned long addref() ROCKET_NOEXCEPT
        {
            return ++count;
        }

        unsigned long release() ROCKET_NOEXCEPT
        {
            return --count;
        }

        unsigned long get() const ROCKET_NOEXCEPT
        {
            return count;
        }

    private:
        unsigned long count{ 0 };
    };

    struct ref_count_atomic
    {
        unsigned long addref() ROCKET_NOEXCEPT
        {
            return ++count;
        }

        unsigned long release() ROCKET_NOEXCEPT
        {
            return --count;
        }

        unsigned long get() const ROCKET_NOEXCEPT
        {
            return count.load();
        }

    private:
        std::atomic<unsigned long> count{ 0 };
    };

    template <class Class, class RefCount = ref_count>
    struct ref_counted
    {
        ref_counted() ROCKET_NOEXCEPT = default;

        ref_counted(ref_counted const&) ROCKET_NOEXCEPT
        {
        }

        ref_counted& operator = (ref_counted const&) ROCKET_NOEXCEPT
        {
            return *this;
        }

        void addref() ROCKET_NOEXCEPT
        {
            count.addref();
        }

        void release() ROCKET_NOEXCEPT
        {
            if (count.release() == 0) {
                delete static_cast<Class*>(this);
            }
        }

    protected:
        ~ref_counted() ROCKET_NOEXCEPT = default;

    private:
        RefCount count{};
    };

    template <class T>
    class stable_list
    {
        struct link_element : ref_counted<link_element>
        {
            link_element() ROCKET_NOEXCEPT = default;

            ~link_element() ROCKET_NOEXCEPT
            {
                if (next) {             // If we have a next element upon destruction
                    value()->~T();      // then this link is used, else it's a dummy
                }
            }

            template <class... Args>
            void construct(Args&&... args)
            {
                new (storage()) T{ std::forward<Args>(args)... };
            }

            T* value() ROCKET_NOEXCEPT
            {
                return static_cast<T*>(storage());
            }

            void* storage() ROCKET_NOEXCEPT
            {
                return static_cast<void*>(&buffer);
            }

            intrusive_ptr<link_element> next;
            intrusive_ptr<link_element> prev;

            std::aligned_storage_t<sizeof(T), std::alignment_of<T>::value> buffer;
        };

        intrusive_ptr<link_element> head;
        intrusive_ptr<link_element> tail;

        std::size_t elements;

    public:
        template <class U>
        struct iterator_base
        {
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = std::remove_const_t<U>;
            using difference_type = ptrdiff_t;
            using reference = U&;
            using pointer = U*;

            template <class V> friend class stable_list;

            iterator_base() ROCKET_NOEXCEPT = default;
            ~iterator_base() ROCKET_NOEXCEPT = default;

            iterator_base(iterator_base const& i) ROCKET_NOEXCEPT
                : element{ i.element }
            {
            }

            iterator_base(iterator_base&& i) ROCKET_NOEXCEPT
                : element{ std::move(i.element) }
            {
            }

            template <class V>
            explicit iterator_base(iterator_base<V> const& i) ROCKET_NOEXCEPT
                : element{ i.element }
            {
            }

            template <class V>
            explicit iterator_base(iterator_base<V>&& i) ROCKET_NOEXCEPT
                : element{ std::move(i.element) }
            {
            }

            iterator_base& operator = (iterator_base const& i) ROCKET_NOEXCEPT
            {
                element = i.element;
                return *this;
            }

            iterator_base& operator = (iterator_base&& i) ROCKET_NOEXCEPT
            {
                element = std::move(i.element);
                return *this;
            }

            template <class V>
            iterator_base& operator = (iterator_base<V> const& i) ROCKET_NOEXCEPT
            {
                element = i.element;
                return *this;
            }

            template <class V>
            iterator_base& operator = (iterator_base<V>&& i) ROCKET_NOEXCEPT
            {
                element = std::move(i.element);
                return *this;
            }

            iterator_base& operator ++ () ROCKET_NOEXCEPT
            {
                element = element->next;
                return *this;
            }

            iterator_base operator ++ (int) ROCKET_NOEXCEPT
            {
                iterator_base i{ *this };
                ++(*this);
                return i;
            }

            iterator_base& operator -- () ROCKET_NOEXCEPT
            {
                element = element->prev;
                return *this;
            }

            iterator_base operator -- (int) ROCKET_NOEXCEPT
            {
                iterator_base i{ *this };
                --(*this);
                return i;
            }

            reference operator * () const ROCKET_NOEXCEPT
            {
                return *element->value();
            }

            pointer operator -> () const ROCKET_NOEXCEPT
            {
                return element->value();
            }

            template <class V>
            bool operator == (iterator_base<V> const& i) const ROCKET_NOEXCEPT
            {
                return element == i.element;
            }

            template <class V>
            bool operator != (iterator_base<V> const& i) const ROCKET_NOEXCEPT
            {
                return element != i.element;
            }

        private:
            intrusive_ptr<link_element> element;

            iterator_base(link_element* p) ROCKET_NOEXCEPT
                : element{ p }
            {
            }
        };

        using value_type = T;
        using reference = T&;
        using pointer = T*;
        using const_pointer = const T*;

        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        using iterator = iterator_base<T>;
        using const_iterator = iterator_base<T const>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        stable_list()
        {
            init();
        }

        ~stable_list()
        {
            destroy();
        }

        stable_list(stable_list const& l)
        {
            init();
            insert(end(), l.begin(), l.end());
        }

        stable_list(stable_list&& l)
            : head{ std::move(l.head) }
            , tail{ std::move(l.tail) }
            , elements{ l.elements }
        {
            l.init();
        }

        stable_list(std::initializer_list<value_type> l)
        {
            init();
            insert(end(), l.begin(), l.end());
        }

        template <class Iterator>
        stable_list(Iterator ibegin, Iterator iend)
        {
            init();
            insert(end(), ibegin, iend);
        }

        explicit stable_list(size_type count, value_type const& value)
        {
            init();
            insert(end(), count, value);
        }

        explicit stable_list(size_type count)
        {
            init();
            insert(end(), count, value_type{});
        }

        stable_list& operator = (stable_list const& l)
        {
            if (this != &l) {
                clear();
                insert(end(), l.begin(), l.end());
            }
            return *this;
        }

        stable_list& operator = (stable_list&& l)
        {
            destroy();
            head = std::move(l.head);
            tail = std::move(l.tail);
            elements = l.elements;
            l.init();
            return *this;
        }

        iterator begin() ROCKET_NOEXCEPT
        {
            return iterator{ head->next };
        }

        iterator end() ROCKET_NOEXCEPT
        {
            return iterator{ tail };
        }

        const_iterator begin() const ROCKET_NOEXCEPT
        {
            return const_iterator{ head->next };
        }

        const_iterator end() const ROCKET_NOEXCEPT
        {
            return const_iterator{ tail };
        }

        const_iterator cbegin() const ROCKET_NOEXCEPT
        {
            return const_iterator{ head->next };
        }

        const_iterator cend() const ROCKET_NOEXCEPT
        {
            return const_iterator{ tail };
        }

        reverse_iterator rbegin() ROCKET_NOEXCEPT
        {
            return reverse_iterator{ end() };
        }

        reverse_iterator rend() ROCKET_NOEXCEPT
        {
            return reverse_iterator{ begin() };
        }

        const_reverse_iterator rbegin() const ROCKET_NOEXCEPT
        {
            return const_reverse_iterator{ cend() };
        }

        const_reverse_iterator rend() const ROCKET_NOEXCEPT
        {
            return const_reverse_iterator{ cbegin() };
        }

        const_reverse_iterator crbegin() const ROCKET_NOEXCEPT
        {
            return const_reverse_iterator{ cend() };
        }

        const_reverse_iterator crend() const ROCKET_NOEXCEPT
        {
            return const_reverse_iterator{ cbegin() };
        }

        reference front() ROCKET_NOEXCEPT
        {
            return *begin();
        }

        reference back() ROCKET_NOEXCEPT
        {
            return *rbegin();
        }

        value_type const& front() const ROCKET_NOEXCEPT
        {
            return *cbegin();
        }

        value_type const& back() const ROCKET_NOEXCEPT
        {
            return *crbegin();
        }

        bool empty() const ROCKET_NOEXCEPT
        {
            return cbegin() == cend();
        }

        void clear() ROCKET_NOEXCEPT
        {
            erase(begin(), end());
        }

        void push_front(value_type const& value)
        {
            insert(begin(), value);
        }

        void push_front(value_type&& value)
        {
            insert(begin(), std::move(value));
        }

        void push_back(value_type const& value)
        {
            insert(end(), value);
        }

        void push_back(value_type&& value)
        {
            insert(end(), std::move(value));
        }

        template <class... Args>
        reference emplace_front(Args&&... args)
        {
            return *emplace(begin(), std::forward<Args>(args)...);
        }

        template <class... Args>
        reference emplace_back(Args&&... args)
        {
            return *emplace(end(), std::forward<Args>(args)...);
        }

        void pop_front() ROCKET_NOEXCEPT
        {
            head->next = head->next->next;
            head->next->prev = head;
            --elements;
        }

        void pop_back() ROCKET_NOEXCEPT
        {
            tail->prev = tail->prev->prev;
            tail->prev->next = tail;
            --elements;
        }

        iterator insert(iterator const& pos, value_type const& value)
        {
            return iterator{ make_link(pos.element, value) };
        }

        iterator insert(iterator const& pos, value_type&& value)
        {
            return iterator{ make_link(pos.element, std::move(value)) };
        }

        template <class Iterator>
        iterator insert(iterator const& pos, Iterator ibegin, Iterator iend)
        {
            iterator iter{ end() };
            while (ibegin != iend) {
                iterator tmp{ insert(pos, *ibegin++) };
                if (iter == end()) {
                    iter = std::move(tmp);
                }
            }
            return iter;
        }

        iterator insert(iterator const& pos, std::initializer_list<value_type> l)
        {
            return insert(pos, l.begin(), l.end());
        }

        iterator insert(iterator const& pos, size_type count, value_type const& value)
        {
            iterator iter{ end() };
            for (size_type i = 0; i < count; ++i) {
                iterator tmp{ insert(pos, value) };
                if (iter == end()) {
                    iter = std::move(tmp);
                }
            }
            return iter;
        }

        template <class... Args>
        iterator emplace(iterator const& pos, Args&&... args)
        {
            return iterator{ make_link(pos.element, std::forward<Args>(args)...) };
        }

        void append(value_type const& value)
        {
            insert(end(), value);
        }

        void append(value_type&& value)
        {
            insert(end(), std::move(value));
        }

        template <class Iterator>
        void append(Iterator ibegin, Iterator iend)
        {
            insert(end(), ibegin, iend);
        }

        void append(std::initializer_list<value_type> l)
        {
            insert(end(), std::move(l));
        }

        void append(size_type count, value_type const& value)
        {
            insert(end(), count, value);
        }

        void assign(size_type count, value_type const& value)
        {
            clear();
            append(count, value);
        }

        template <class Iterator>
        void assign(Iterator ibegin, Iterator iend)
        {
            clear();
            append(ibegin, iend);
        }

        void assign(std::initializer_list<value_type> l)
        {
            clear();
            append(std::move(l));
        }

        void resize(size_type count)
        {
            resize(count, value_type{});
        }

        void resize(size_type count, value_type const& value)
        {
            size_type cursize = size();
            if (count > cursize) {
                for (size_type i = cursize; i < count; ++i) {
                    push_back(value);
                }
            } else {
                for (size_type i = count; i < cursize; ++i) {
                    pop_back();
                }
            }
        }

        size_type size() const ROCKET_NOEXCEPT
        {
            return elements;
        }

        size_type max_size() const ROCKET_NOEXCEPT
        {
            return std::numeric_limits<size_type>::max();
        }

        iterator erase(iterator const& pos) ROCKET_NOEXCEPT
        {
            pos.element->prev->next = pos.element->next;
            pos.element->next->prev = pos.element->prev;
            --elements;
            return iterator{ pos.element->next };
        }

        iterator erase(iterator const& first, iterator const& last) ROCKET_NOEXCEPT
        {
            auto link = first.element;
            while (link != last.element) {
                auto next = link->next;
                link->prev = first.element->prev;
                link->next = last.element;
                --elements;
                link = std::move(next);
            }

            first.element->prev->next = last.element;
            last.element->prev = first.element->prev;
            return last;
        }

        void remove(value_type const& value) ROCKET_NOEXCEPT
        {
            for (auto itr = begin(); itr != end(); ++itr) {
                if (*itr == value) {
                    erase(itr);
                }
            }
        }

        template <class Predicate>
        void remove_if(Predicate const& pred)
        {
            for (auto itr = begin(); itr != end(); ++itr) {
                if (pred(*itr)) {
                    erase(itr);
                }
            }
        }

        void swap(stable_list& other) ROCKET_NOEXCEPT
        {
            if (this != &other) {
                head.swap(other.head);
                tail.swap(other.tail);
                std::swap(elements, other.elements);
            }
        }

    private:
        void init()
        {
            head = new link_element;
            tail = new link_element;
            head->next = tail;
            tail->prev = head;
            elements = 0;
        }

        void destroy()
        {
            clear();
            head->next = nullptr;
            tail->prev = nullptr;
        }

        template <class... Args>
        link_element* make_link(link_element* l, Args&&... args)
        {
            intrusive_ptr<link_element> link{ new link_element };
            link->construct(std::forward<Args>(args)...);
            link->prev = l->prev;
            link->next = l;
            link->prev->next = link;
            link->next->prev = link;
            ++elements;
            return link;
        }
    };

    template <bool ThreadSafe>
    struct threading_policy
    {
        const bool is_thread_safe{ ThreadSafe };
    };

    struct thread_safe_policy : threading_policy<true> {};
    struct thread_unsafe_policy : threading_policy<false> {};

    namespace detail
    {
        template <class>
        struct expand_signature;

        template <class R, class... Args>
        struct expand_signature<R(Args...)>
        {
            using return_type = R;
            using signature_type = R(Args...);
        };

        template <class Signature>
        using get_return_type = typename expand_signature<Signature>::return_type;

        struct shared_lock : ref_counted<shared_lock, ref_count_atomic>
        {
            std::mutex mutex;
        };

        template <class ThreadingPolicy>
        struct shared_lock_state;

        template <>
        struct shared_lock_state<thread_unsafe_policy>
        {
            constexpr void lock() ROCKET_NOEXCEPT
            {
            }

            constexpr bool try_lock() ROCKET_NOEXCEPT
            {
                return true;
            }

            constexpr void unlock() ROCKET_NOEXCEPT
            {
            }

            constexpr void swap(shared_lock_state& s) ROCKET_NOEXCEPT
            {
            }
        };

        template <>
        struct shared_lock_state<thread_safe_policy>
        {
            shared_lock_state()
                : lock_primitive{ new shared_lock }
            {
            }

            ~shared_lock_state() = default;

            shared_lock_state(shared_lock_state const& s)
                : lock_primitive{ s.lock_primitive }
            {
            }

            shared_lock_state(shared_lock_state&& s)
                : lock_primitive{ std::move(s.lock_primitive) }
            {
                s.lock_primitive = new shared_lock;
            }

            shared_lock_state& operator = (shared_lock_state const& rhs)
            {
                lock_primitive = rhs.lock_primitive;
                return *this;
            }

            shared_lock_state& operator = (shared_lock_state&& rhs)
            {
                lock_primitive = std::move(rhs.lock_primitive);
                rhs.lock_primitive = new shared_lock;
                return *this;
            }

            void lock()
            {
                lock_primitive->mutex.lock();
            }

            bool try_lock()
            {
                return lock_primitive->mutex.try_lock();
            }

            void unlock() 
            {
                lock_primitive->mutex.unlock();
            }

            void swap(shared_lock_state& s) ROCKET_NOEXCEPT
            {
                lock_primitive.swap(s.lock_primitive);
            }

            intrusive_ptr<shared_lock> lock_primitive;
        };

        template <class ThreadingPolicy>
        struct connection_base;

        template <>
        struct connection_base<thread_unsafe_policy>
            : thread_unsafe_policy
            , ref_counted<connection_base<thread_unsafe_policy>>
        {
            virtual ~connection_base() ROCKET_NOEXCEPT = default;

            bool is_connected() const ROCKET_NOEXCEPT
            {
                return prev != nullptr;
            }

            void disconnect() ROCKET_NOEXCEPT
            {
                if (prev != nullptr) {
                    next->prev = prev;
                    prev->next = next;

                    // To mark a connection as disconnected, just set its prev-link to null but
                    // leave the next link alive so we can still traverse through the connections
                    // if the slot gets disconnected during signal emit.
                    prev = nullptr;
                }
            }

            std::thread::id get_tid() const ROCKET_NOEXCEPT
            {
                return std::thread::id{};
            }

            constexpr bool is_queued() const ROCKET_NOEXCEPT
            {
                return false;
            }

            intrusive_ptr<connection_base> next;
            intrusive_ptr<connection_base> prev;

            bool is_blocked{ false };
        };

        template <>
        struct connection_base<thread_safe_policy>
            : thread_safe_policy
            , ref_counted<connection_base<thread_safe_policy>, ref_count_atomic>
        {
            virtual ~connection_base() ROCKET_NOEXCEPT = default;

            bool is_connected() const ROCKET_NOEXCEPT
            {
                return static_cast<intrusive_ptr<connection_base> volatile const&>(prev).get() != nullptr;
            }

            void disconnect() ROCKET_NOEXCEPT
            {
                std::scoped_lock<std::mutex> guard{ lock->mutex };

                if (prev != nullptr) {
                    next->prev = prev;
                    prev->next = next;

                    // To mark a connection as disconnected, just set its prev-link to null but
                    // leave the next link alive so we can still traverse through the connections
                    // if the slot gets disconnected during signal emit.
                    static_cast<intrusive_ptr<connection_base> volatile&>(prev) = nullptr;
                }
            }

            std::thread::id const& get_tid() const ROCKET_NOEXCEPT
            {
                return thread_id;
            }

            bool is_queued() const ROCKET_NOEXCEPT
            {
                return thread_id != std::thread::id{}
                    && thread_id != std::this_thread::get_id();
            }

            intrusive_ptr<connection_base> next;
            intrusive_ptr<connection_base> prev;

            intrusive_ptr<shared_lock> lock;

            std::thread::id thread_id;

            volatile bool is_blocked{ false };
        };

        template <class ThreadingPolicy, class T>
        struct functional_connection : connection_base<ThreadingPolicy>
        {
            std::function<T> slot;
        };

        // Should make sure that this is POD
        struct thread_local_data
        {
            void* current_connection;
            bool emission_aborted;
        };

        inline thread_local_data* get_thread_local_data() ROCKET_NOEXCEPT
        {
            static ROCKET_THREAD_LOCAL thread_local_data th;
            return &th;
        }

        struct connection_scope
        {
            connection_scope(void* base, thread_local_data* th) ROCKET_NOEXCEPT
                : th{ th }
                , prev{ th->current_connection }
            {
                th->current_connection = base;
            }

            ~connection_scope() ROCKET_NOEXCEPT
            {
                th->current_connection = prev;
            }

            thread_local_data* th;
            void* prev;
        };

        struct abort_scope
        {
            abort_scope(thread_local_data* th) ROCKET_NOEXCEPT
                : th{ th }
                , prev{ th->emission_aborted }
            {
                th->emission_aborted = false;
            }

            ~abort_scope() ROCKET_NOEXCEPT
            {
                th->emission_aborted = prev;
            }

            thread_local_data* th;
            bool prev;
        };

        template <class Instance, class Class, class R, class... Args>
        struct weak_mem_fn
        {
            explicit weak_mem_fn(std::weak_ptr<Instance> c, R(Class::*method)(Args...))
                : weak{ std::move(c) }
                , method{ method }
            {
            }

            template <class... Args1>
            auto operator () (Args1&&... args) const
            {
                if constexpr (std::is_void_v<R>) {
                    if (auto strong = weak.lock()) {
                        (strong.get()->*method)(std::forward<Args1>(args)...);
                    }
                } else {
                    if (auto strong = weak.lock()) {
                        return optional<R>{
                            (strong.get()->*method)(std::forward<Args1>(args)...)
                        };
                    }
                    return optional<R>{};
                }
            }

        private:
            std::weak_ptr<Instance> weak;
            R(Class::*method)(Args...);
        };

        template <class Instance, class Class, class R, class... Args>
        struct shared_mem_fn
        {
            explicit shared_mem_fn(std::shared_ptr<Instance> c, R(Class::*method)(Args...))
                : shared{ std::move(c) }
                , method{ method }
            {
            }

            template <class... Args1>
            R operator () (Args1&&... args) const
            {
                return (shared.get()->*method)(std::forward<Args1>(args)...);
            }

        private:
            std::shared_ptr<Instance> shared;
            R(Class::*method)(Args...);
        };

        struct call_queue
        {
            void put(std::thread::id tid, std::packaged_task<void()> task)
            {
                std::scoped_lock<std::mutex> guard{ mutex };
                queue[tid].push_front(std::move(task));
            }

            void dispatch()
            {
                std::thread::id tid = std::this_thread::get_id();
                std::forward_list<std::packaged_task<void()>> thread_queue;

                {
                    std::scoped_lock<std::mutex> guard{ mutex };

                    auto iterator = queue.find(tid);
                    if (iterator != queue.end()) {
                        thread_queue.swap(iterator->second);
                        queue.erase(iterator);
                    }
                }

                for (auto& function : thread_queue) {
                    function();
                }
            }

        private:
            std::mutex mutex;
            std::unordered_map<std::thread::id, std::forward_list<std::packaged_task<void()>>> queue;
        };

        inline call_queue* get_call_queue() ROCKET_NOEXCEPT
        {
            static call_queue queue;
            return &queue;
        }
    }

    template <class Instance, class Class, class R, class... Args>
    auto bind_weak_ptr(std::weak_ptr<Instance> c, R(Class::*method)(Args...))
    {
        return detail::weak_mem_fn<Instance, Class, R, Args...>{ std::move(c), method };
    }

    template <class Instance, class Class, class R, class... Args>
    auto bind_weak_ptr(std::shared_ptr<Instance> c, R(Class::*method)(Args...))
    {
        return detail::weak_mem_fn<Instance, Class, R, Args...>{ std::move(c), method };
    }

    template <class Instance, class Class, class R, class... Args>
    auto bind_shared_ptr(std::shared_ptr<Instance> c, R(Class::*method)(Args...))
    {
        return detail::shared_mem_fn<Instance, Class, R, Args...>{ std::move(c), method };
    }

    inline void dispatch_queued_calls()
    {
        detail::get_call_queue()->dispatch();
    }

    struct connection
    {
        connection() ROCKET_NOEXCEPT
            : base{ nullptr }
        {
        }

        ~connection() ROCKET_NOEXCEPT
        {
            release();
        }

        connection(connection&& rhs) ROCKET_NOEXCEPT
            : base{ rhs.base }
        {
            rhs.base = nullptr;
        }

        connection(connection const& rhs) ROCKET_NOEXCEPT
            : base{ rhs.base }
        {
            addref();
        }

        explicit connection(void* base) ROCKET_NOEXCEPT
            : base{ base }
        {
            addref();
        }

        connection& operator = (connection&& rhs) ROCKET_NOEXCEPT
        {
            release();
            base = rhs.base;
            rhs.base = nullptr;
            return *this;
        }

        connection& operator = (connection const& rhs) ROCKET_NOEXCEPT
        {
            if (this != &rhs) {
                release();
                base = rhs.base;
                addref();
            }
            return *this;
        }

        bool operator == (connection const& rhs) const ROCKET_NOEXCEPT
        {
            return base == rhs.base;
        }

        bool operator != (connection const& rhs) const ROCKET_NOEXCEPT
        {
            return base != rhs.base;
        }

        explicit operator bool() const ROCKET_NOEXCEPT
        {
            return is_connected();
        }

        bool is_connected() const ROCKET_NOEXCEPT
        {
            if (base != nullptr) {
                auto safe{ static_cast<detail::connection_base<thread_safe_policy>*>(base) };
                auto unsafe{ static_cast<detail::connection_base<thread_unsafe_policy>*>(base) };

                assert(&safe->is_thread_safe == &unsafe->is_thread_safe);

                if (!unsafe->is_thread_safe) [[likely]] {
                    return unsafe->is_connected();
                } else {
                    return safe->is_connected();
                }
            }
            return false;
        }

        bool is_blocked() const ROCKET_NOEXCEPT
        {
            if (base != nullptr) {
                auto safe{ static_cast<detail::connection_base<thread_safe_policy>*>(base) };
                auto unsafe{ static_cast<detail::connection_base<thread_unsafe_policy>*>(base) };

                assert(&safe->is_thread_safe == &unsafe->is_thread_safe);

                if (!unsafe->is_thread_safe) [[likely]] {
                    return unsafe->is_blocked;
                } else {
                    return safe->is_blocked;
                }
            }
            return false;
        }

        void block() ROCKET_NOEXCEPT
        {
            if (base != nullptr) {
                auto safe{ static_cast<detail::connection_base<thread_safe_policy>*>(base) };
                auto unsafe{ static_cast<detail::connection_base<thread_unsafe_policy>*>(base) };

                assert(&safe->is_thread_safe == &unsafe->is_thread_safe);

                if (!unsafe->is_thread_safe) [[likely]] {
                    unsafe->is_blocked = true;
                } else {
                    std::scoped_lock<std::mutex> guard{ safe->lock->mutex };
                    safe->is_blocked = true;
                }
            }
        }

        void unblock() ROCKET_NOEXCEPT
        {
            if (base != nullptr) {
                auto safe{ static_cast<detail::connection_base<thread_safe_policy>*>(base) };
                auto unsafe{ static_cast<detail::connection_base<thread_unsafe_policy>*>(base) };

                assert(&safe->is_thread_safe == &unsafe->is_thread_safe);

                if (!unsafe->is_thread_safe) [[likely]] {
                    unsafe->is_blocked = false;
                } else {
                    safe->is_blocked = false;
                }
            }
        }

        void disconnect() ROCKET_NOEXCEPT
        {
            if (base != nullptr) {
                auto safe{ static_cast<detail::connection_base<thread_safe_policy>*>(base) };
                auto unsafe{ static_cast<detail::connection_base<thread_unsafe_policy>*>(base) };

                assert(&safe->is_thread_safe == &unsafe->is_thread_safe);

                if (!unsafe->is_thread_safe) [[likely]] {
                    unsafe->disconnect();
                } else {
                    safe->disconnect();
                }

                release();
            }
        }

        void swap(connection& other) ROCKET_NOEXCEPT
        {
            if (this != &other) {
                void* tmp_base{ base };
                base = other.base;
                other.base = tmp_base;
            }
        }

    private:
        void* base;

        friend struct scoped_connection_blocker;

        void addref() ROCKET_NOEXCEPT
        {
            if (base != nullptr) {
                auto safe{ static_cast<detail::connection_base<thread_safe_policy>*>(base) };
                auto unsafe{ static_cast<detail::connection_base<thread_unsafe_policy>*>(base) };

                assert(&safe->is_thread_safe == &unsafe->is_thread_safe);

                if (!unsafe->is_thread_safe) [[likely]] {
                    unsafe->addref();
                } else {
                    safe->addref();
                }
            }
        }

        void release() ROCKET_NOEXCEPT
        {
            if (base != nullptr) {
                auto safe{ static_cast<detail::connection_base<thread_safe_policy>*>(base) };
                auto unsafe{ static_cast<detail::connection_base<thread_unsafe_policy>*>(base) };

                assert(&safe->is_thread_safe == &unsafe->is_thread_safe);

                if (!unsafe->is_thread_safe) [[likely]] {
                    unsafe->release();
                } else {
                    safe->release();
                }

                base = nullptr;
            }
        }
    };

    struct scoped_connection : connection
    {
        scoped_connection() ROCKET_NOEXCEPT = default;

        ~scoped_connection() ROCKET_NOEXCEPT
        {
            disconnect();
        }

        scoped_connection(connection const& rhs) ROCKET_NOEXCEPT
            : connection{ rhs }
        {
        }

        scoped_connection(connection&& rhs) ROCKET_NOEXCEPT
            : connection{ std::move(rhs) }
        {
        }

        scoped_connection(scoped_connection&& rhs) ROCKET_NOEXCEPT
            : connection{ std::move(rhs) }
        {
        }

        scoped_connection& operator = (connection&& rhs) ROCKET_NOEXCEPT
        {
            disconnect();

            connection::operator=(std::move(rhs));
            return *this;
        }

        scoped_connection& operator = (scoped_connection&& rhs) ROCKET_NOEXCEPT
        {
            disconnect();

            connection::operator=(std::move(rhs));
            return *this;
        }

        scoped_connection& operator = (connection const& rhs) ROCKET_NOEXCEPT
        {
            disconnect();

            connection::operator=(rhs);
            return *this;
        }

    private:
        scoped_connection(scoped_connection const&) = delete;

        scoped_connection& operator = (scoped_connection const&) = delete;
    };

    struct scoped_connection_container
    {
        scoped_connection_container() = default;
        ~scoped_connection_container() = default;

        scoped_connection_container(scoped_connection_container&& s)
            : connections{ std::move(s.connections) }
        {
        }

        scoped_connection_container& operator = (scoped_connection_container&& rhs)
        {
            connections = std::move(rhs.connections);
            return *this;
        }

        scoped_connection_container(std::initializer_list<connection> list)
        {
            append(list);
        }

        void append(connection const& conn)
        {
            connections.push_front(scoped_connection{ conn });
        }

        void append(std::initializer_list<connection> list)
        {
            for (auto const& connection : list) {
                append(connection);
            }
        }

        scoped_connection_container& operator += (connection const& conn)
        {
            append(conn);
            return *this;
        }

        scoped_connection_container& operator += (std::initializer_list<connection> list)
        {
            for (auto const& connection : list) {
                append(connection);
            }
            return *this;
        }

        void disconnect() ROCKET_NOEXCEPT
        {
            connections.clear();
        }

    private:
        scoped_connection_container(scoped_connection_container const&) = delete;
        scoped_connection_container& operator = (scoped_connection_container const&) = delete;

        std::forward_list<scoped_connection> connections;
    };

    struct trackable
    {
        void add_tracked_connection(connection const& conn)
        {
            container.append(conn);
        }

        void disconnect_tracked_connections() ROCKET_NOEXCEPT
        {
            container.disconnect();
        }

    private:
        scoped_connection_container container;
    };

    inline connection current_connection() ROCKET_NOEXCEPT
    {
        return connection{ detail::get_thread_local_data()->current_connection };
    }

    inline void abort_emission() ROCKET_NOEXCEPT
    {
        detail::get_thread_local_data()->emission_aborted = true;
    }

    struct scoped_connection_blocker
    {
        scoped_connection_blocker(connection& c)
        {
            if (c.base != nullptr) {
                auto safe{ static_cast<detail::connection_base<thread_safe_policy>*>(c.base) };
                auto unsafe{ static_cast<detail::connection_base<thread_unsafe_policy>*>(c.base) };

                assert(&safe->is_thread_safe == &unsafe->is_thread_safe);

                if (!unsafe->is_thread_safe) [[likely]] {
                    if (!unsafe->is_blocked) [[likely]] {
                        unsafe->is_blocked = true;
                        connection = &c;
                    }
                } else {
                    std::scoped_lock<std::mutex> guard{ safe->lock->mutex };

                    if (!safe->is_blocked) [[likely]] {
                        safe->is_blocked = true;
                        connection = &c;
                    }
                }
            }
        }

        ~scoped_connection_blocker()
        {
            if (connection != nullptr) {
                connection->unblock();
            }
        }

    private:
        scoped_connection_blocker(scoped_connection_blocker const&) = delete;
        scoped_connection_blocker& operator = (scoped_connection_blocker const&) = delete;

        connection* connection{ nullptr };
    };

    template <class T>
    struct default_collector : last<optional<T>>
    {
    };

    template <>
    struct default_collector<void>
    {
        using value_type = void;
        using result_type = void;

        void operator () () ROCKET_NOEXCEPT
        {
            /* do nothing for void types */
        }

        void result() ROCKET_NOEXCEPT
        {
            /* do nothing for void types */
        }
    };

    enum connection_flags : unsigned int
    {
        direct_connection = 0,
        queued_connection = 1 << 0,
        connect_as_first_slot = 1 << 1,
    };

    template <class Signature
        , class Collector = default_collector<detail::get_return_type<Signature>>
        , class ThreadingPolicy = thread_unsafe_policy>
    struct signal;

    template <class Collector, class ThreadingPolicy, class R, class... Args>
    struct signal<R(Args...), Collector, ThreadingPolicy>
    {
        using signature_type = R(Args...);
        using slot_type = std::function<signature_type>;

        signal()
        {
            init();
        }

        ~signal() ROCKET_NOEXCEPT
        {
            std::scoped_lock<shared_lock_state> guard{ lock_state };
            destroy();
        }

        signal(signal&& s)
        {
            static_assert(std::is_same_v<ThreadingPolicy, thread_unsafe_policy>, "Thread safe signals can't be moved or swapped.");

            head = std::move(s.head);
            tail = std::move(s.tail);
            s.init();
        }

        signal(signal const& s)
        {
            init();

            std::scoped_lock<shared_lock_state> guard{ s.lock_state };
            copy(s);
        }

        signal& operator = (signal&& rhs)
        {
            static_assert(std::is_same_v<ThreadingPolicy, thread_unsafe_policy>, "Thread safe signals can't be moved or swapped.");

            destroy();
            head = std::move(rhs.head);
            tail = std::move(rhs.tail);
            rhs.init();
            return *this;
        }

        signal& operator = (signal const& rhs)
        {
            if (this != &rhs) {
                std::scoped_lock<shared_lock_state, shared_lock_state> guard{ lock_state, rhs.lock_state };
                clear_without_lock();
                copy(rhs);
            }
            return *this;
        }

        connection connect(slot_type slot, connection_flags flags = direct_connection)
        {
            assert(slot != nullptr);

            std::thread::id tid{};

            bool first = (flags & connect_as_first_slot) != 0;
            
            if constexpr (std::is_same_v<ThreadingPolicy, thread_safe_policy>) {
                if ((flags & queued_connection) != 0) {
                    tid = std::this_thread::get_id();
                }
            } else {
                assert((flags & queued_connection) == 0);
            }

            std::scoped_lock<shared_lock_state> guard{ lock_state };
            connection_base* base = make_link(first ? head->next : tail, std::move(slot), tid);

            return connection{ static_cast<void*>(base) };
        }

        template <class R1, class... Args1>
        connection connect(R1(*method)(Args1...), connection_flags flags = direct_connection)
        {
            return connect([method](Args const&... args) {
                return R((*method)(Args1(args)...));
            }, flags);
        }

        template <auto Method>
        connection connect(connection_flags flags = direct_connection)
        {
            return connect([](Args const&... args) {
                return R((*Method)(args...));
            }, flags);
        }

        template <class Instance, class Class, class R1, class... Args1>
        connection connect(Instance& object, R1(Class::*method)(Args1...), connection_flags flags = direct_connection)
        {
            connection c{
                connect([&object, method](Args const&... args) {
                    return R((object.*method)(Args1(args)...));
                }, flags)
            };
            if constexpr (std::is_convertible_v<Instance*, trackable*>) {
                static_cast<trackable&>(object).add_tracked_connection(c);
            }
            return c;
        }

        template <auto Method, class Instance>
        connection connect(Instance& object, connection_flags flags = direct_connection)
        {
            connection c{
                connect([&object](Args const&... args) {
                    return R((object.*Method)(args...));
                }, flags)
            };
            if constexpr (std::is_convertible_v<Instance*, trackable*>) {
                static_cast<trackable&>(object).add_tracked_connection(c);
            }
            return c;
        }

        template <class Instance, class Class, class R1, class... Args1>
        connection connect(Instance* object, R1(Class::*method)(Args1...), connection_flags flags = direct_connection)
        {
            connection c{
                connect([object, method](Args const&... args) {
                    return R((object->*method)(Args1(args)...));
                }, flags)
            };
            if constexpr (std::is_convertible_v<Instance*, trackable*>) {
                static_cast<trackable*>(object)->add_tracked_connection(c);
            }
            return c;
        }

        template <auto Method, class Instance>
        connection connect(Instance* object, connection_flags flags = direct_connection)
        {
            connection c{
                connect([object](Args const&... args) {
                    return R((object->*Method)(args...));
                }, flags)
            };
            if constexpr (std::is_convertible_v<Instance*, trackable*>) {
                static_cast<trackable*>(object)->add_tracked_connection(c);
            }
            return c;
        }

        connection operator += (slot_type slot)
        {
            return connect(std::move(slot));
        }

        void clear() ROCKET_NOEXCEPT
        {
            std::scoped_lock<shared_lock_state> guard{ lock_state };
            clear_without_lock();
        }

        void swap(signal& other) ROCKET_NOEXCEPT
        {
            static_assert(std::is_same_v<ThreadingPolicy, thread_unsafe_policy>, "Thread safe signals can't be moved or swapped.");

            if (this != &other) {
                head.swap(other.head);
                tail.swap(other.tail);
            }
        }

        template <class ValueCollector = Collector>
        auto invoke(Args const&... args) const
        {
#ifndef ROCKET_NO_EXCEPTIONS
            bool error{ false };
#endif
            ValueCollector collector{};
            {
                detail::thread_local_data* th{ detail::get_thread_local_data() };
                detail::abort_scope ascope{ th };

                lock_state.lock();

                intrusive_ptr<connection_base> current{ head->next };
                intrusive_ptr<connection_base> end{ tail };

                while (current != end) {
                    assert(current != nullptr);

                    if (current->is_connected() && !current->is_blocked) {
                        detail::connection_scope cscope{ current, th };

                        lock_state.unlock();

                        functional_connection* conn = static_cast<
                            functional_connection*>(static_cast<void*>(current));

                        if constexpr (std::is_same_v<ThreadingPolicy, thread_unsafe_policy>) {
#ifndef ROCKET_NO_EXCEPTIONS
                            try {
#endif
                                if constexpr (std::is_void_v<R>) {
                                    conn->slot(args...); collector();
                                } else {
                                    collector(conn->slot(args...));
                                }
#ifndef ROCKET_NO_EXCEPTIONS
                            } catch (...) {
                                error = true;
                            }
#endif
                        } else {
                            if (current->is_queued()) {
                                std::packaged_task<void()> task([current, &collector, args...] {
                                    if (current->is_connected()) {
                                        detail::thread_local_data* th{ detail::get_thread_local_data() };
                                        detail::connection_scope cscope{ current, th };

                                        functional_connection* conn = static_cast<
                                            functional_connection*>(static_cast<void*>(current));

                                        if constexpr (std::is_void_v<R>) {
                                            conn->slot(args...);
                                        } else {
                                            collector(conn->slot(args...));
                                        }
                                    }
                                });

                                std::future<void> future{ task.get_future() };
                                detail::get_call_queue()->put(current->get_tid(), std::move(task));

                                if constexpr (!std::is_void_v<R>) {
#ifndef ROCKET_NO_EXCEPTIONS
                                    try {
#endif
                                        future.get();
#ifndef ROCKET_NO_EXCEPTIONS
                                    } catch (...) {
                                        error = true;
                                    }
#endif
                                }
                            } else {
#ifndef ROCKET_NO_EXCEPTIONS
                                try {
#endif
                                    if constexpr (std::is_void_v<R>) {
                                        conn->slot(args...); collector();
                                    } else {
                                        collector(conn->slot(args...));
                                    }
#ifndef ROCKET_NO_EXCEPTIONS
                                } catch (...) {
                                    error = true;
                                }
#endif
                            }
                        }

                        lock_state.lock();

                        if (th->emission_aborted) {
                            break;
                        }
                    }

                    current = current->next;
                }

                lock_state.unlock();
            }

#ifndef ROCKET_NO_EXCEPTIONS
            if (error) {
                throw invocation_slot_error{};
            }
#endif
            return collector.result();
        }

        auto operator () (Args const&... args) const
        {
            return invoke(args...);
        }

    private:
        using shared_lock_state = detail::shared_lock_state<ThreadingPolicy>;
        using connection_base = detail::connection_base<ThreadingPolicy>;
        using functional_connection = detail::functional_connection<ThreadingPolicy, signature_type>;

        void init()
        {
            head = new connection_base;
            tail = new connection_base;
            head->next = tail;
            tail->prev = head;
        }

        void destroy() ROCKET_NOEXCEPT
        {
            clear_without_lock();
            head->next = nullptr;
            tail->prev = nullptr;
        }

        void clear_without_lock() ROCKET_NOEXCEPT
        {
            intrusive_ptr<connection_base> current{ head->next };
            while (current != tail) {
                intrusive_ptr<connection_base> next{ current->next };
                current->next = tail;
                current->prev = nullptr;
                current = std::move(next);
            }

            head->next = tail;
            tail->prev = head;
        }

        void copy(signal const& s)
        {
            intrusive_ptr<connection_base> current{ s.head->next };
            intrusive_ptr<connection_base> end{ s.tail };

            while (current != end) {
                functional_connection* conn = static_cast<
                    functional_connection*>(static_cast<void*>(current));

                make_link(tail, conn->slot, conn->get_tid());
                current = current->next;
            }
        }

        functional_connection* make_link(connection_base* l, slot_type slot, std::thread::id tid)
        {
            intrusive_ptr<functional_connection> link{ new functional_connection };

            if constexpr (std::is_same_v<ThreadingPolicy, thread_safe_policy>) {
                link->lock = lock_state.lock_primitive;
                link->thread_id = std::move(tid);
            }

            link->slot = std::move(slot);
            link->prev = l->prev;
            link->next = l;
            link->prev->next = link;
            link->next->prev = link;
            return link;
        }

        intrusive_ptr<connection_base> head;
        intrusive_ptr<connection_base> tail;

        mutable shared_lock_state lock_state;
    };

    template <class Signature
        , class Collector = default_collector<detail::get_return_type<Signature>>>
    using thread_safe_signal = signal<Signature, Collector, thread_safe_policy>;

    template <class Instance, class Class, class R, class... Args>
    std::function<R(Args...)> slot(Instance& object, R(Class::*method)(Args...))
    {
        return [&object, method](Args const&... args) {
            return (object.*method)(args...);
        };
    }

    template <class Instance, class Class, class R, class... Args>
    std::function<R(Args...)> slot(Instance* object, R(Class::*method)(Args...))
    {
        return [object, method](Args const&... args) {
            return (object->*method)(args...);
        };
    }

    template <class T>
    void swap(intrusive_ptr<T>& p1, intrusive_ptr<T>& p2) ROCKET_NOEXCEPT
    {
        p1.swap(p2);
    }

    template <class T>
    void swap(stable_list<T>& l1, stable_list<T>& l2) ROCKET_NOEXCEPT
    {
        l1.swap(l2);
    }

    void swap(connection& c1, connection& c2) ROCKET_NOEXCEPT
    {
        c1.swap(c2);
    }

    template <class Signature, class Collector, class ThreadingPolicy>
    void swap(signal<Signature, Collector, ThreadingPolicy>& s1, signal<Signature, Collector, ThreadingPolicy>& s2) ROCKET_NOEXCEPT
    {
        s1.swap(s2);
    }
}

#endif
