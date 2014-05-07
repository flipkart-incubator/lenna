package flipkart.rukmini;

import flipkart.rukmini.healthcheck.DummyHealthCheck;
import flipkart.rukmini.resources.PingResource;
import flipkart.rukmini.resources.ScaledImageResource;
import flipkart.rukmini.resources.StatusResource;
import io.dropwizard.Application;
import io.dropwizard.assets.AssetsBundle;
import io.dropwizard.setup.Bootstrap;
import io.dropwizard.setup.Environment;

/**
 * Main class for bootstrapping server
 */
public class Rukmini extends Application<RukminiConfiguration> {

    public static void main(String args[]) throws Exception {
        new Rukmini().run(args);
    }

    @Override
    public void initialize(Bootstrap<RukminiConfiguration> bootstrap) {
        bootstrap.addBundle(new AssetsBundle());
    }

    @Override
    public void run(RukminiConfiguration configuration, Environment environment) throws Exception {
        environment.healthChecks().register("dummy", new DummyHealthCheck());
        environment.jersey().register(new PingResource());
        environment.jersey().register(new StatusResource());
        environment.jersey().register(new ScaledImageResource(configuration));
    }
}
