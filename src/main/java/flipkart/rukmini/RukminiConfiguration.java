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

    @JsonProperty
    public String getCdn() {
        return cdn;
    }

    @JsonProperty
    public void setCdn(String cdn) {
        this.cdn = cdn;
    }

}
