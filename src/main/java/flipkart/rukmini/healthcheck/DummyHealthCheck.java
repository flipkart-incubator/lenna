package flipkart.rukmini.healthcheck;

import com.codahale.metrics.health.HealthCheck;

/**
 * A dummy health check to stop dropwizard from complaining
 */
public class DummyHealthCheck extends HealthCheck {

    @Override
    protected Result check() throws Exception {
        return Result.healthy();
    }
}
