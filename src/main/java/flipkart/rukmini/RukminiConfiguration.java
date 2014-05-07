package flipkart.rukmini;

import com.fasterxml.jackson.annotation.JsonProperty;
import io.dropwizard.Configuration;
import org.hibernate.validator.constraints.NotEmpty;

/**
 * Configuration for application
 */
public class RukminiConfiguration extends Configuration {

    @NotEmpty
    private String cdn;

    @NotEmpty
    private String mode;

    @JsonProperty
    public String getCdn() {
        return cdn;
    }

    @JsonProperty
    public void setCdn(String cdn) {
        this.cdn = cdn;
    }

    @JsonProperty
    public String getMode() {
        return mode;
    }

    @JsonProperty
    public void setMode(String mode) {
        this.mode = mode;
    }

}
