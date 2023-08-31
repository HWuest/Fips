/*
 * DHS_EventHandler.hpp
 *
 *  Created on: 08.05.2023
 *      Author: HW
 *
 * Usage:
 * 1. #include "DHS_EventHanler.hpp"
 * 2. Define Eventhandler function:
 *    DHS::event_handler <char> handler([](char c) { // char handler function
 * 			printf("handle %c\n",c);
 *	  });
 * 3. Define list of handlers: DHS::event <char> handlers;
 * 4. Add handler to list: hanlders += handler;
 * 5. Call all handlers in list with 'x': handlers.call('x');
 *
 */

#ifndef DHS_EVENTHANDLER_H_
#define DHS_EVENTHANDLER_H_

#include <functional>
#include <list>
#include "esp_log.h"
#include <stdio.h>

#include "timer_u32.h"

namespace DHS {

  // event handler class template
  template <typename... Args> class event_handler
  {
	public:

		typedef std::function<void(Args...)> handler_func_type; // event handler function type definition
		typedef unsigned int handler_id_type; // event handler id type definition
		typedef unsigned int handler_type_type; // event handler id type definition

		explicit event_handler(const handler_func_type& handlerFunc) // constructor to set handler function
			: m_handlerFunc(handlerFunc) // set function pointer
		{
			m_handlerId = ++m_handlerIdCounter; // increment and set handler id
			m_handlerType = 0;
		}

		// copy constructor
		event_handler(const event_handler& src)
			: m_handlerFunc(src.m_handlerFunc), m_handlerId(src.m_handlerId), m_handlerType(src.m_handlerType) {}

		// move constructor
		event_handler(event_handler&& src)
			: m_handlerFunc(std::move(src.m_handlerFunc)), m_handlerId(src.m_handlerId), m_handlerType(src.m_handlerType) {}

		// copy assignment operator
		event_handler& operator=(const event_handler& src)
		{
			m_handlerFunc = src.m_handlerFunc;
			m_handlerId = src.m_handlerId;
			m_handlerType = src.m_handlerType;
			return *this;
		}

		// move assignment operator
		event_handler& operator=(event_handler&& src)
		{
			std::swap(m_handlerFunc, src.m_handlerFunc);
			m_handlerId = src.m_handlerId;
			m_handlerType = src.m_handlerType;
			return *this;
		}

		// function call operator
		void operator()(Args... params) const
		{
			if (m_handlerFunc)
			{
				m_handlerFunc(params...);
			}
		}

		// compare operator
		bool operator==(const event_handler& other) const
		{
			return m_handlerId == other.m_handlerId;
		}

		// bool operator
		operator bool() const
		{
			return m_handlerFunc;
		}

		// get event handler id
		handler_id_type id() const
		{
			return m_handlerId;
		}

	    void setType(uint8_t t) { m_handlerType = t; }
	    uint8_t getType() const { return m_handlerType; }

	private:
		handler_func_type m_handlerFunc; // event handler function pointer
		handler_id_type m_handlerId; // event handler id
		handler_type_type m_handlerType;

	    static unsigned int m_handlerIdCounter; // global id counter
  };

  template <typename... Args> unsigned int event_handler<Args...>::m_handlerIdCounter(0); // initialize id counter


  // event list class template
  template <typename... Args> class event
	{
	public:
		typedef event_handler<Args...> handler_type; // handler type definition
		typedef std::list<handler_type> handler_collection_type; // event handler list type definition

		handler_collection_type m_handlers; // list of event handlers

		void setActiveType(uint8_t t) {
			activeType = t;
		}

		// empty constructor
		event() { activeType = 255;	}

		// copy constructor
		event(const event& src)	{
			m_handlers = src.m_handlers;
		}

		// move constructor
		event(event&& src)	{
			m_handlers = std::move(src.m_handlers);
		}

		// copy assignment operator
		event& operator=(const event& src)	{
			m_handlers = src.m_handlers;

			return *this;
		}

		// move assignment operator
		event& operator=(event&& src)	{
			std::swap(m_handlers, src.m_handlers);

			return *this;
		}

		// add event handler to list
		typename handler_type::handler_id_type add(const handler_type& handler)
		{
			m_handlers.push_back(handler);

			return handler.id();
		}

		// add event handler to list
		inline typename handler_type::handler_id_type add(const typename handler_type::handler_func_type& handler)
		{
			return add(handler_type(handler));
		}

		// remove event handler from list
		bool remove(const handler_type& handler)
		{
			auto it = std::find(m_handlers.begin(), m_handlers.end(), handler);
			if (it != m_handlers.end())
			{
				m_handlers.erase(it);
				return true;
			}

			return false;
		}

		// get handler type by id
		typename handler_type::handler_type_type getType(const typename handler_type::handler_id_type& handlerId)
		{   unsigned int t;
			auto it = std::find_if(m_handlers.begin(), m_handlers.end(),
				[handlerId,&t](const handler_type& handler) { t = handler.getType(); return handler.id() == handlerId; });
			if (it != m_handlers.end()) return t;
			return 0;
		}

		// set handler type by id
		bool setType(const typename handler_type::handler_id_type& handlerId, const typename handler_type::handler_type_type t)
		{
			auto it = std::find_if(m_handlers.begin(), m_handlers.end(),
				[handlerId,t](const handler_type& handler) { if (handler.id() == handlerId) handler.setType(t); return handler.id() == handlerId; });
			if (it != m_handlers.end()) return true;
			return false;
		}

		// remove event handler from list by id
		bool remove_id(const typename handler_type::handler_id_type& handlerId)
		{
			auto it = std::find_if(m_handlers.begin(), m_handlers.end(),
				[handlerId](const handler_type& handler) { return handler.id() == handlerId; });
			if (it != m_handlers.end())
			{
				m_handlers.erase(it);
				return true;
			}

			return false;
		}

		// call all registerd event handler functions (with handler list copy, save if list changes during calls)
		void call(Args... params) const
		{
			handler_collection_type handlersCopy = get_handlers_copy();

			call_impl(handlersCopy, params...);
		}

		// call all registerd event handler functions (without handler list copy)
		void call_impl(const handler_collection_type& handlers, Args... params) const
		{
			//uint32_t s,d;
			//s = timer_u32();
			for (const auto& handler : handlers)
			{
				if ((activeType & handler.getType())== handler.getType()) { // active (1) bit patterns match?
					handler(params...); // call active handlers depending on type bits
				}
			  //d = timer_u32()-s;
			  //printf(" %f\n",timer_delta_ms(d));
			}
		}

		// function operator to call event handler
		inline void operator()(Args... params) const
		{
			call(params...);
		}

		// size function to get number of registered event handlers
		inline int size() const
		{
			return m_handlers.size();
		}

		// + operator to add event handler
		inline typename handler_type::handler_id_type operator+=(const handler_type& handler)
		{
			return add(handler);
		}

		// + operator to add event handler
		inline typename handler_type::handler_id_type operator+=(const typename handler_type::handler_func_type& handler)
		{
			return add(handler);
		}

		// - operator to remove event handler
		inline bool operator-=(const handler_type& handler)
		{
			return remove(handler);
		}

	private:
	    uint8_t activeType;
	protected:
		// get a copy of event handler list
		handler_collection_type get_handlers_copy() const
		{
			return m_handlers;
		}

  };

} // end of DHS namespace

#endif /* DHS_EVENTHANDLER_H_ */
