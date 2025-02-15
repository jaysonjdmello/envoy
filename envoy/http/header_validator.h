#pragma once

#include <string>
#include <tuple>

#include "envoy/config/typed_config.h"
#include "envoy/http/header_map.h"
#include "envoy/http/protocol.h"
#include "envoy/protobuf/message_validator.h"

namespace Envoy {
namespace Http {

/**
 * Interface for header validators.
 */
class HeaderValidator {
public:
  virtual ~HeaderValidator() = default;

  // A class that holds either success condition or an error condition with tuple of
  // action and error details.
  template <typename ActionType> class Result {
  public:
    using Action = ActionType;

    // Helper for constructing successful results
    static Result success() { return Result(ActionType::Accept, absl::string_view()); }

    Result(ActionType action, absl::string_view details) : result_(action, details) {
      ENVOY_BUG(action == ActionType::Accept || !details.empty(),
                "Error details must not be empty in case of an error");
    }

    bool ok() const { return std::get<0>(result_) == ActionType::Accept; }
    operator bool() const { return ok(); }
    absl::string_view details() const { return std::get<1>(result_); }
    Action action() const { return std::get<0>(result_); }

  private:
    const std::tuple<ActionType, std::string> result_;
  };

  enum class RejectAction { Accept, Reject };
  enum class RejectOrRedirectAction { Accept, Reject, Redirect };
  using RejectResult = Result<RejectAction>;
  using RejectOrRedirectResult = Result<RejectOrRedirectAction>;

  /**
   * Validate the entire request header map.
   * Returning the Reject value form this method causes the HTTP request to be rejected with 400
   * status, and the gRPC request with the INTERNAL (13) error code.
   */
  using ValidationResult = RejectResult;
  virtual ValidationResult validateRequestHeaders(const RequestHeaderMap& header_map) PURE;

  /**
   * Transform the entire request header map.
   * This method transforms the header map, for example by normalizing URI path, before processing
   * by the filter chain.
   * Returning the Reject value from this method causes the HTTP request to be rejected with 400
   * status, and the gRPC request with the INTERNAL (13) error code. Returning the Redirect
   * value causes the HTTP request to be redirected to the :path presudo header in the request map.
   * The gRPC request will still be rejected with the INTERNAL (13) error code.
   */
  using HeadersTransformationResult = RejectOrRedirectResult;
  virtual HeadersTransformationResult transformRequestHeaders(RequestHeaderMap& header_map) PURE;

  /**
   * Validate the entire response header map.
   * Returning the Reject value causes the HTTP request to be rejected with the 502 status,
   * and the gRPC request with the UNAVAILABLE (14) error code.
   */
  virtual ValidationResult validateResponseHeaders(const ResponseHeaderMap& header_map) PURE;

  /**
   * Validate the entire request trailer map.
   * Returning the Reject value causes the HTTP request to be rejected with the 502 status,
   * and the gRPC request with the UNAVAILABLE (14) error code.
   * If response headers have already been sent the request is reset.
   */
  virtual ValidationResult validateRequestTrailers(const RequestTrailerMap& trailer_map) PURE;

  /**
   * Transform the entire request trailer map.
   * Returning the Reject value causes the HTTP request to be rejected with the 502 status,
   * and the gRPC request with the UNAVAILABLE (14) error code.
   * If response headers have already been sent the request is reset.
   */
  using TrailersTransformationResult = RejectResult;
  virtual TrailersTransformationResult transformRequestTrailers(RequestTrailerMap& header_map) PURE;

  /**
   * Validate the entire response trailer map.
   * Returning the Reject value causes the HTTP request to be reset.
   */
  virtual ValidationResult validateResponseTrailers(const ResponseTrailerMap& trailer_map) PURE;
};

using HeaderValidatorPtr = std::unique_ptr<HeaderValidator>;

/**
 * Interface for stats.
 */
class HeaderValidatorStats {
public:
  virtual ~HeaderValidatorStats() = default;

  virtual void incDroppedHeadersWithUnderscores() PURE;
  virtual void incRequestsRejectedWithUnderscoresInHeaders() PURE;
  virtual void incMessagingError() PURE;
};

/**
 * Interface for creating header validators.
 */
class HeaderValidatorFactory {
public:
  virtual ~HeaderValidatorFactory() = default;

  /**
   * Create a new header validator for the specified protocol.
   */
  virtual HeaderValidatorPtr create(Protocol protocol, HeaderValidatorStats& stats) PURE;
};

using HeaderValidatorFactoryPtr = std::unique_ptr<HeaderValidatorFactory>;

/**
 * Extension configuration for header validators.
 */
class HeaderValidatorFactoryConfig : public Config::TypedFactory {
public:
  virtual HeaderValidatorFactoryPtr
  createFromProto(const Protobuf::Message& config,
                  ProtobufMessage::ValidationVisitor& validation_visitor) PURE;

  std::string category() const override { return "envoy.http.header_validators"; }
};

} // namespace Http
} // namespace Envoy
