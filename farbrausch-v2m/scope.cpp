#include "scope.h"

#include <vector>

/**
* Simple "scope" impl used to visualize the inner workings of the sound generation.
*
* Based on the interface already used in Farbrausch's original impl, but rendering
* IS NOT performed here so the API is actually reduced to copy the streams into
* respective buffers - that will be used elsewhere.
*
* The idea is that the buffers use the exact same size as the audio buffer
* and that they are actually in sync.
*
* 2019 (C) by Jürgen Wothke
*/

static const int MAXSCOPES = 16;

int scope_activated = 0;

namespace
{
  class Scope
  {
    std::vector<float> samples;
    size_t pos;

    char scope_title[256];

  public:
    const void *unique_id;

    Scope(const void *uniq, int size, const char *title, int w, int h)
    {
      unique_id = uniq;
      samples.resize(size);
      pos = 0;
      
      strcpy(scope_title, title);
    }

    ~Scope()
    {
    }

	const char* get_title() {
		return scope_title;
	}
	
	float* get_output_buffer() {
		return &samples[0];
	}
	
    void submit(const float *data, size_t stride, size_t count)
    {
      size_t cur = 0;
      while (cur < count)
      {
        size_t block = std::min(count - cur, samples.size() - pos);
        for (size_t i=0; i < block; i++)
          samples[pos+i] = data[(cur+i) * stride];
        cur += block;
        pos = (pos + block) % samples.size();
      }	  
    }

    void render()
    {
		// nothing to do.. rendering happends elsewhere
    }
  };
}

static Scope *scopes[MAXSCOPES];

static float* scope_buffers[16];	// for now just use one per voice
static const char* scope_titles[16];

static Scope *get_scope(const void *unique_id)
{
  for (int i=0; i < MAXSCOPES; i++)
    if (scopes[i] && scopes[i]->unique_id == unique_id)
      return scopes[i];

  return 0;
}

float** get_scope_buffers()
{
  for (int i=0; i < MAXSCOPES; i++)
    if (scopes[i])
      scope_buffers[i]= scopes[i]->get_output_buffer();
	else 
      scope_buffers[i]= 0;

  return scope_buffers;
}

const char** get_scope_titles()
{
  for (int i=0; i < MAXSCOPES; i++)
    if (scopes[i])
      scope_titles[i]= scopes[i]->get_title();
	else 
      scope_titles[i]= 0;

  return scope_titles;
}

void scopeOpen(const void *unique_id, const char *title, int nsamples, int w, int h)
{
  if (get_scope(unique_id))
    scopeClose(unique_id);

  for (int i=0; i < MAXSCOPES; i++)
  {
    if (!scopes[i])
    {
      scopes[i] = new Scope(unique_id, nsamples, title, w, h);
      break;
    }
  }
}

void scopeClose(const void *unique_id)
{
  for (int i=0; i < MAXSCOPES; i++)
  {
    if (scopes[i] && scopes[i]->unique_id == unique_id)
    {
      delete scopes[i];
      scopes[i] = 0;
    }
  }
}

bool scopeIsOpen(const void *unique_id)
{
    return get_scope(unique_id) != 0;
}
void scopeSubmit(const void *unique_id, const float *data, int nsamples)
{
  Scope *s = get_scope(unique_id);
  if (scope_activated && s) {
    s->submit(data, 1, nsamples);	// typical: nsamples = 128	
  }
}

void scopeSubmitStrided(const void *unique_id, const float *data, int stride, int nsamples)
{
  if (Scope *s = get_scope(unique_id))	// e.g. used for interleaved stereo data
    s->submit(data, stride, nsamples);
}

void scopeUpdateAll()
{
	/* useless with no render() 
	
  for (int i=0; i < MAXSCOPES; i++)
  {
    if (!scopes[i]) {		
      continue;
	}

    scopes[i]->render();
  }
  */
}
