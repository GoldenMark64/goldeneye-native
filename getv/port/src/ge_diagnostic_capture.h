#ifndef GE_DIAGNOSTIC_CAPTURE_H
#define GE_DIAGNOSTIC_CAPTURE_H

#ifdef __cplusplus
extern "C" {
#endif

/* F3 requests one local, review-only capture. The request is consumed by the current
 * rendered frame; repeated requests are ignored until that frame has been finalized. */
void gePortDiagnosticRequest(void);

/* Called once after a rendered frame has completed and game state is settled. */
void gePortDiagnosticFinalizeFrame(unsigned long render_frame);

/* Renderers query this before the developer overlay is drawn. NULL means no manual capture. */
const char *gePortDiagnosticScreenshotPath(void);

/* Renderer reports whether the requested native BMP was written successfully. */
void gePortDiagnosticScreenshotComplete(int success, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* GE_DIAGNOSTIC_CAPTURE_H */
