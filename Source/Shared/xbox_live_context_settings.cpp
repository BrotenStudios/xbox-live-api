﻿// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include "shared_macros.h"
#include "xsapi/system.h"
#include "xbox_system_factory.h"
#if XSAPI_A
#include "Logger/android/logcat_output.h"
#else
#include "Logger/debug_output.h"
#endif
NAMESPACE_MICROSOFT_XBOX_SERVICES_CPP_BEGIN

#if TV_API || UWP_API || UNIT_TEST_SERVICES
Windows::UI::Core::CoreDispatcher^ xbox_live_context_settings::_s_dispatcher;
#endif

#if UWP_API || UNIT_TEST_SERVICES
void
xbox_live_context_settings::_Set_dispatcher(
    _In_opt_ Platform::Object^ coreDispatcherObj
    )
{
    if (coreDispatcherObj != nullptr)
    {
        _s_dispatcher = dynamic_cast<Windows::UI::Core::CoreDispatcher^>(coreDispatcherObj);
    }
    else // if no dispatcher passed in, try to get use direct caller's dispatcher.
    {
        try
        {
#if !UNIT_TEST_SERVICES
            auto currentView = Windows::ApplicationModel::Core::CoreApplication::GetCurrentView();
            if (currentView != nullptr && currentView->CoreWindow != nullptr)
            {
                _s_dispatcher = currentView->CoreWindow->Dispatcher;
            }
#endif
        }
        catch (Platform::Exception^ ex)
        {
            // Get caller's dispatcher failed. Move on with empty dispatcher.
        }
    }

    if (_s_dispatcher != nullptr)
    {
        _s_dispatcher->RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            ref new Windows::UI::Core::DispatchedHandler([]()
        {
            XBOX_LIVE_NAMESPACE::utils::generate_locales();
        }));
    }
}
#endif

#if (XSAPI_SERVER || UNIT_TEST_SYSTEM)

cert_context xbox_live_context_settings::_s_certContext;

/// <summary>
/// Sets the SSL Cert
/// </summary>
void xbox_live_context_settings::_Set_SSL_cert(_In_ cert_context cert)
{
    if (cert != nullptr)
    {
        _s_certContext = cert;
    }
};

#endif

xbox_live_context_settings::xbox_live_context_settings() :
    m_enableServiceCallRoutedEvents(false),
    m_httpTimeout(std::chrono::seconds(DEFAULT_HTTP_TIMEOUT_SECONDS)),
    m_longHttpTimeout(std::chrono::seconds(DEFAULT_LONG_HTTP_TIMEOUT_SECONDS)),
    m_websocketTimeoutWindow(std::chrono::seconds(DEFAULT_WEBSOCKET_TIMEOUT_SECONDS)),
    m_httpRetryDelay(std::chrono::seconds(DEFAULT_RETRY_DELAY_SECONDS)),
    m_httpTimeoutWindow(std::chrono::seconds(DEFAULT_HTTP_RETRY_WINDOW_SECONDS)),
    m_useCoreDispatcherForEventRouting(false),
    m_disableAssertsForXboxLiveThrottlingInDevSandboxes(false),
    m_disableAssertsForMaxNumberOfWebsocketsActivated(false)
{
}

function_context xbox_live_context_settings::add_service_call_routed_handler(_In_ std::function<void(const XBOX_LIVE_NAMESPACE::xbox_service_call_routed_event_args&)> handler)
{
    std::lock_guard<std::mutex> lock(m_writeLock);

    function_context context = -1;
    if (handler != nullptr)
    {
        context = ++m_serviceCallRoutedHandlersCounter;
        m_serviceCallRoutedHandlers[m_serviceCallRoutedHandlersCounter] = std::move(handler);
    }

    return context;
}

void xbox_live_context_settings::remove_service_call_routed_handler(_In_ function_context context)
{
    std::lock_guard<std::mutex> lock(m_writeLock);
    m_serviceCallRoutedHandlers.erase(context);
}

bool xbox_live_context_settings::enable_service_call_routed_events() const
{
    return m_enableServiceCallRoutedEvents;
}

xbox_services_diagnostics_trace_level xbox_live_context_settings::diagnostics_trace_level() const
{
    return system::xbox_live_services_settings::get_singleton_instance()->diagnostics_trace_level();
}

void xbox_live_context_settings::set_diagnostics_trace_level(_In_ xbox_services_diagnostics_trace_level value)
{
    system::xbox_live_services_settings::get_singleton_instance()->set_diagnostics_trace_level(value);
}

void xbox_live_context_settings::_Raise_service_call_routed_event(_In_ const XBOX_LIVE_NAMESPACE::xbox_service_call_routed_event_args& result)
{
    std::lock_guard<std::mutex> lock(m_writeLock);

    for (auto& handler : m_serviceCallRoutedHandlers)
    {
        XSAPI_ASSERT(handler.second != nullptr);
        if (handler.second != nullptr)
        {
            try
            {
                handler.second(result);
            }
            catch (...)
            {
                LOG_ERROR("raise_service_call_routed_event failed.");
            }
        }
    }
}

void xbox_live_context_settings::set_enable_service_call_routed_events(_In_ bool value)
{
    m_enableServiceCallRoutedEvents = value;
}

const std::chrono::seconds& xbox_live_context_settings::http_timeout() const
{
    return m_httpTimeout;
}

void xbox_live_context_settings::set_http_timeout(_In_ std::chrono::seconds value)
{
    m_httpTimeout = std::move(value);
}

const std::chrono::seconds& xbox_live_context_settings::long_http_timeout() const
{
    return m_longHttpTimeout;
}

void xbox_live_context_settings::set_long_http_timeout(_In_ std::chrono::seconds value)
{
    m_longHttpTimeout = std::move(value);
}

const std::chrono::seconds& xbox_live_context_settings::http_retry_delay() const
{
    return m_httpRetryDelay;
}

void xbox_live_context_settings::set_http_retry_delay(_In_ std::chrono::seconds value)
{
    m_httpRetryDelay = std::move(value);
    m_httpRetryDelay = std::chrono::seconds(__max(m_httpRetryDelay.count(), MIN_RETRY_DELAY_SECONDS));
}

const std::chrono::seconds& xbox_live_context_settings::http_timeout_window() const
{
    return m_httpTimeoutWindow;
}

void xbox_live_context_settings::set_http_timeout_window(_In_ std::chrono::seconds value)
{
    m_httpTimeoutWindow = std::move(value);
}

const std::chrono::seconds& xbox_live_context_settings::websocket_timeout_window() const
{
    return m_websocketTimeoutWindow;
}

void xbox_live_context_settings::set_websocket_timeout_window(_In_ std::chrono::seconds value)
{
    m_websocketTimeoutWindow = std::move(value);
}

bool xbox_live_context_settings::use_core_dispatcher_for_event_routing() const
{
    return m_useCoreDispatcherForEventRouting;
}

void xbox_live_context_settings::set_use_core_dispatcher_for_event_routing(_In_ bool value)
{
    m_useCoreDispatcherForEventRouting = value;
}

void xbox_live_context_settings::disable_asserts_for_xbox_live_throttling_in_dev_sandboxes(
    _In_ xbox_live_context_throttle_setting setting
    )
{
    if (setting == xbox_live_context_throttle_setting::this_code_needs_to_be_changed_to_avoid_throttling)
    {
        m_disableAssertsForXboxLiveThrottlingInDevSandboxes = true;
    }
}

bool xbox_live_context_settings::_Is_disable_asserts_for_xbox_live_throttling_in_dev_sandboxes()
{
    return m_disableAssertsForXboxLiveThrottlingInDevSandboxes;
}

void xbox_live_context_settings::disable_asserts_for_maximum_number_of_websockets_activated(
    _In_ xbox_live_context_recommended_setting setting
)
{
    if (setting == xbox_live_context_recommended_setting::this_code_needs_to_be_changed_to_follow_best_practices)
    {
        m_disableAssertsForMaxNumberOfWebsocketsActivated = true;
    }
}

bool xbox_live_context_settings::_Is_disable_asserts_for_max_number_of_websockets_activated()
{
    return m_disableAssertsForMaxNumberOfWebsocketsActivated;
}

NAMESPACE_MICROSOFT_XBOX_SERVICES_CPP_END
