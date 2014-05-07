package flipkart.rukmini.models;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Simple ping response
 */
public class PingResponse {

    public static final String MESSAGE = "pong";

    public PingResponse() {

    }

    @JsonProperty
    public String getMessage() {
        return MESSAGE;
    }
}
