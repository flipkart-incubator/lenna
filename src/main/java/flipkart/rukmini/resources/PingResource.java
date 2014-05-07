package flipkart.rukmini.resources;

import flipkart.rukmini.models.PingResponse;
import io.dropwizard.jersey.caching.CacheControl;

import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.Produces;
import javax.ws.rs.core.MediaType;
import java.util.concurrent.TimeUnit;

/**
 * A simple ping resource
 */
@Path("/")
@Produces(MediaType.APPLICATION_JSON)
public class PingResource {

    @GET
    @Path("ping")
    @CacheControl(maxAge = 1, maxAgeUnit = TimeUnit.DAYS)
    public PingResponse ping() {
        return new PingResponse();
    }

}
