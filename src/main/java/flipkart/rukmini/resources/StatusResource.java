package flipkart.rukmini.resources;

import com.sun.jersey.spi.resource.Singleton;

import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Response;

/**
 * Status Resource for lb health check and oor and inr functionality
 */
@Singleton
@Path("/status")
public class StatusResource {

    private static boolean isOOR = false;

    @GET
    @Path("check")
    public Response status() {
        if(isOOR)
            return Response.status(Response.Status.SERVICE_UNAVAILABLE).build();
        else
            return Response.ok().build();
    }

    @POST
    @Path("oor")
    public Response oor() {
        isOOR = true;
        return Response.ok().build();
    }

    @POST
    @Path("inr")
    public Response inr() {
        isOOR = false;
        return Response.ok().build();
    }

}
