package flipkart.rukmini.resources;

import com.codahale.metrics.annotation.Timed;
import com.google.common.base.Optional;
import com.google.common.hash.Hashing;
import flipkart.rukmini.RukminiConfiguration;
import flipkart.rukmini.helpers.ImageResizeHelper;
import io.dropwizard.jersey.caching.CacheControl;
import org.apache.commons.io.FileUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.ws.rs.*;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.nio.charset.Charset;
import java.util.concurrent.TimeUnit;

/**
 * Scaled image resource
 */
@Path("/image")
public class ScaledImageResource {

    private final Logger log = LoggerFactory.getLogger("ScaledImageResource");

    private RukminiConfiguration configuration;

    public ScaledImageResource(RukminiConfiguration configuration) {
        this.configuration = configuration;
    }

    @GET
    @Path("{width}/{height}/{imageUri:.*}")
    @Produces("image/*")
    @CacheControl(isPrivate = false, maxAge = Integer.MAX_VALUE, maxAgeUnit = TimeUnit.SECONDS, sharedMaxAge = Integer.MAX_VALUE, sharedMaxAgeUnit = TimeUnit.SECONDS)
    @Timed(name = "image-requests")
    public Response scaledImage(@PathParam("width") int width, @PathParam("height") int height,
                                @PathParam("imageUri") String imageUri, @QueryParam("q") Optional<Integer> quality) {
        File fInput;
        File fOutput = null;
        try {
            fInput = download(imageUri);
            if(!fInput.exists())
                return Response.status(Response.Status.NOT_FOUND).type(MediaType.TEXT_PLAIN).entity("Not Found").build();
            fOutput = ImageResizeHelper.resize(fInput.getAbsolutePath(), height, width, quality.or(90),
                    configuration.getMode());
            byte data[] = FileUtils.readFileToByteArray(fOutput);
            return Response.ok(data).header("ETag", Hashing.md5().hashString(imageUri, Charset.defaultCharset())
                    .toString()).type("image/jpeg").build();
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
