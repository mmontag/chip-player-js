export const validate = (schema) => (req, res, next) => {
  const result = schema.safeParse(req.body);

  if (!result.success) {
    console.log(result.error);
    return res.status(400).json({
      error: 'Validation failed',
      // This flattens the Zod error into a readable object
      details: result.error,
    });
  }



  // Replace the body with the parsed/stripped data
  req.body = result.data;
  next();
};
