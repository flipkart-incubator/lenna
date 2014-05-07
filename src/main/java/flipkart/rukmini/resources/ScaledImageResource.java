package flipkart.rukmini.resources;

import com.codahale.metrics.annotation.Timed;
import com.google.common.base.Optional;
import flipkart.rukmini.RukminiConfiguration;
import flipkart.rukmini.helpers.ImageResizeHelper;
import org.apache.commons.io.FileUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.ws.rs.*;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import java.io.File;
import java.io.IOException;
import java.net.URL;

/**
 * Scaled image resource
 */
@Path("/resize")
public class ScaledImageResource {

    private final Logger log = LoggerFactory.getLogger("ScaledImageResource");

    private RukminiConfiguration configuration;

    public ScaledImageResource(RukminiConfiguration configuration) {
        this.configuration = configuration;
    }

    @GET
    @Path("{resolution}/{imageUri:.*}")
    @Produces("image/*")
    @Timed(name = "image-requests")
    public Response scaledImage(@PathParam("resolution") int resolution, @PathParam("imageUri") String imageUri, @QueryParam("q") Optional<Integer> quality) {
        File fInput = null;
        File fOutput = null;
        try {
            fInput = download(imageUri);
            if(!fInput.exists())
                return Response.status(Response.Status.NOT_FOUND).type(MediaType.TEXT_PLAIN).entity("Not Found").build();
            fOutput = ImageResizeHelper.resize(fInput.getAbsolutePath(), resolution, resolution, quality.or(90));
            byte data[] = FileUtils.readFileToByteArray(fOutput);
            return Response.ok(data).type("image/jpeg").build();
        } catch (IOException e) {
            log.error("Error scaling resource: " +e.getMessage(), e);
            return Response.serverError().build();
        } catch (InterruptedException e) {
            log.error("Error scaling resource: " + e.getMessage(), e);
            return Response.serverError().build();
        } finally {
            if(fOutput != null) FileUtils.deleteQuietly(fOutput);
        }
    }

    @Timed(name = "image-downloads")
    private File download(final String imageUri) throws IOException {
        File fTemp = File.createTempFile("download", "img");
        FileUtils.copyURLToFile(new URL(String.format(configuration.getCdn(),imageUri)), fTemp);
        log.debug("Downloaded file size: " + FileUtils.sizeOf(fTemp));
        return fTemp;
    }

}
