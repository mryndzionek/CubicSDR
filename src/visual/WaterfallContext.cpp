#include "WaterfallContext.h"
#include "WaterfallCanvas.h"
#include "CubicSDR.h"

WaterfallContext::WaterfallContext(WaterfallCanvas *canvas, wxGLContext *sharedContext) :
        PrimaryGLContext(canvas, sharedContext), waterfall_lines(0), fft_size(0), activeTheme(NULL) {
    for (int i = 0; i < 2; i++) {
        waterfall[i] = 0;
        waterfall_tex[i] = 0;
    }
}

void WaterfallContext::Setup(int fft_size_in, int num_waterfall_lines_in) {
    waterfall_lines = num_waterfall_lines_in;
    fft_size = fft_size_in;

    int half_fft_size = fft_size / 2;

    for (int i = 0; i < 2; i++) {
        if (waterfall[i]) {
            glDeleteTextures(1, &waterfall[i]);
            waterfall[i] = 0;
        }
        if (waterfall_tex[i]) {
            delete waterfall_tex[i];
        }

        waterfall_tex[i] = new unsigned char[half_fft_size * waterfall_lines * 2];
        memset(waterfall_tex[i], 0, half_fft_size * waterfall_lines * 2);
    }
    // Stagger memory updates at half intervals for tiles
    waterfall_ofs[0] = waterfall_lines;
    waterfall_ofs[1] = waterfall_lines - waterfall_lines / 8;
}

void WaterfallContext::refreshTheme() {
    glEnable(GL_TEXTURE_2D);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, waterfall[i]);

        glPixelTransferi(GL_MAP_COLOR, GL_TRUE);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 256, &(ThemeMgr::mgr.currentTheme->waterfallGradient.getRed())[0]);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_G, 256, &(ThemeMgr::mgr.currentTheme->waterfallGradient.getGreen())[0]);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_B, 256, &(ThemeMgr::mgr.currentTheme->waterfallGradient.getBlue())[0]);
    }
}

void WaterfallContext::Draw(std::vector<float> &points) {

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

    if (!waterfall[0]) {
        glGenTextures(2, waterfall);

        for (int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, waterfall[i]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    }

    if (activeTheme != ThemeMgr::mgr.currentTheme) {
        refreshTheme();
        activeTheme = ThemeMgr::mgr.currentTheme;
    }

    int half_fft_size = fft_size / 2;

    if (points.size()) {
        for (int j = 0; j < 2; j++) {

            int ofs = waterfall_ofs[j];
            for (int i = 0, iMax = half_fft_size; i < iMax; i++) {
                float v = points[(j * half_fft_size + i) * 2 + 1];

                float wv = v < 0 ? 0 : (v > 0.99 ? 0.99 : v);

                waterfall_tex[j][i + ofs * half_fft_size] = (unsigned char) floor(wv * 255.0);
            }

            int quarter_lines = (waterfall_lines / 4);
            int k = 4;
            while (k--) {
                if (waterfall_ofs[j] == quarter_lines * k) {
                    memcpy(waterfall_tex[j] + (waterfall_lines * half_fft_size) + (quarter_lines * k * half_fft_size),
                            waterfall_tex[j] + (quarter_lines * k * half_fft_size), quarter_lines * half_fft_size);
                }
            }

            if (waterfall_ofs[j] == 0) {
                waterfall_ofs[j] = waterfall_lines;
            }

            glBindTexture(GL_TEXTURE_2D, waterfall[j]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, half_fft_size, waterfall_lines, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
                    (GLvoid *) (waterfall_tex[j] + (half_fft_size * (waterfall_ofs[j]))));

            waterfall_ofs[j]--;
        }
    }

    glColor3f(1.0, 1.0, 1.0);

    GLint vp[4];
    glGetIntegerv( GL_VIEWPORT, vp);

    float viewWidth = (float) vp[2];

    // some bias to prevent seams at odd scales
    float half_pixel = 1.0 / (float) viewWidth;
    float half_texel = 1.0 / (float) half_fft_size;

    glBindTexture(GL_TEXTURE_2D, waterfall[0]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0 + half_texel, 1.0 - half_texel);
    glVertex3f(-1.0, -1.0, 0.0);
    glTexCoord2f(1.0 - half_texel, 1.0 - half_texel);
    glVertex3f(0.0 + half_pixel, -1.0, 0.0);
    glTexCoord2f(1.0 - half_texel, 0.0 + half_texel);
    glVertex3f(0.0 + half_pixel, 1.0, 0.0);
    glTexCoord2f(0.0 + half_texel, 0.0 + half_texel);
    glVertex3f(-1.0, 1.0, 0.0);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, waterfall[1]);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0 + half_texel, 1.0 - half_texel);
    glVertex3f(0.0 - half_pixel, -1.0, 0.0);
    glTexCoord2f(1.0 - half_texel, 1.0 - half_texel);
    glVertex3f(1.0, -1.0, 0.0);
    glTexCoord2f(1.0 - half_texel, 0.0 + half_texel);
    glVertex3f(1.0, 1.0, 0.0);
    glTexCoord2f(0.0 + half_texel, 0.0 + half_texel);
    glVertex3f(0.0 - half_pixel, 1.0, 0.0);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);

}
